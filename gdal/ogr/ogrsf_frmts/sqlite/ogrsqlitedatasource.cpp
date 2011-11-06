/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 *
 * Contributor: Alessandro Furieri, a.furieri@lqt.it
 * Portions of this module properly supporting SpatiaLite Table/Geom creation
 * Developed for Faunalia ( http://www.faunalia.it) with funding from 
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_hash_set.h"

#ifdef HAVE_SPATIALITE
#include "spatialite.h"
#endif

static int bSpatialiteLoaded = FALSE;

CPL_CVSID("$Id$");

/************************************************************************/
/*                       OGRSQLiteHasSpatialite()                       */
/************************************************************************/

int OGRSQLiteInitSpatialite()
{
/* -------------------------------------------------------------------- */
/*      Try loading SpatiaLite.                                         */
/* -------------------------------------------------------------------- */
#ifdef HAVE_SPATIALITE
    if (!bSpatialiteLoaded && CSLTestBoolean(CPLGetConfigOption("SPATIALITE_LOAD", "TRUE")))
    {
        bSpatialiteLoaded = TRUE;
        spatialite_init(CSLTestBoolean(CPLGetConfigOption("SPATIALITE_INIT_VERBOSE", "FALSE")));
    }
#endif
    return bSpatialiteLoaded;
}

/************************************************************************/
/*               OGRSQLiteGetSpatialiteVersionNumber()                  */
/************************************************************************/

int OGRSQLiteGetSpatialiteVersionNumber()
{
    double v = 0.0;
#ifdef HAVE_SPATIALITE
    v = ( atof( spatialite_version() ) + 0.001 )  * 10.0;
#endif
    return (int)v;
}

/************************************************************************/
/*                        OGRSQLiteDataSource()                         */
/************************************************************************/

OGRSQLiteDataSource::OGRSQLiteDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;

    nSoftTransactionLevel = 0;

    nKnownSRID = 0;
    panSRID = NULL;
    papoSRS = NULL;

    bHaveGeometryColumns = FALSE;
    bIsSpatiaLite = FALSE;
    bUpdate = FALSE;

#ifdef HAVE_SQLITE_VFS
    pMyVFS = NULL;
#endif
}

/************************************************************************/
/*                        ~OGRSQLiteDataSource()                        */
/************************************************************************/

OGRSQLiteDataSource::~OGRSQLiteDataSource()

{
    int         i;

    CPLFree( pszName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );

    for( i = 0; i < nKnownSRID; i++ )
    {
        if( papoSRS[i] != NULL )
            papoSRS[i]->Release();
    }
    CPLFree( panSRID );
    CPLFree( papoSRS );

    if( hDB != NULL )
        sqlite3_close( hDB );

#ifdef HAVE_SQLITE_VFS
    if (pMyVFS)
    {
        sqlite3_vfs_unregister(pMyVFS);
        CPLFree(pMyVFS);
    }
#endif
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Note, the Open() will implicitly create the database if it      */
/*      does not already exist.                                         */
/************************************************************************/

int OGRSQLiteDataSource::Open( const char * pszNewName, int bUpdateIn )

{
    CPLAssert( nLayers == 0 );

    pszName = CPLStrdup( pszNewName );
    bUpdate = bUpdateIn;

    OGRSQLiteInitSpatialite();

    int bListAllTables = CSLTestBoolean(CPLGetConfigOption("SQLITE_LIST_ALL_TABLES", "NO"));

/* -------------------------------------------------------------------- */
/*      Try to open the sqlite database properly now.                   */
/* -------------------------------------------------------------------- */
    int rc;

    hDB = NULL;
    
#ifdef HAVE_SQLITE_VFS
    int bUseOGRVFS = CSLTestBoolean(CPLGetConfigOption("SQLITE_USE_OGR_VFS", "NO"));
    if (bUseOGRVFS || strncmp(pszName, "/vsi", 4) == 0)
    {
        pMyVFS = OGRSQLiteCreateVFS();
        sqlite3_vfs_register(pMyVFS, 0);
        rc = sqlite3_open_v2( pszNewName, &hDB, (bUpdateIn) ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY, pMyVFS->zName );
    }
    else
        rc = sqlite3_open_v2( pszNewName, &hDB, (bUpdateIn) ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY, NULL );
#else
    rc = sqlite3_open( pszNewName, &hDB );
#endif
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "sqlite3_open(%s) failed: %s", 
                  pszNewName, sqlite3_errmsg( hDB ) );

        return FALSE;
    }

    char *pszErrMsg = NULL;

    const char* pszSqliteSync = CPLGetConfigOption("OGR_SQLITE_SYNCHRONOUS", NULL);
    if (pszSqliteSync != NULL)
    {
        if (EQUAL(pszSqliteSync, "OFF") || EQUAL(pszSqliteSync, "0") ||
            EQUAL(pszSqliteSync, "FALSE"))
            rc = sqlite3_exec( hDB, "PRAGMA synchronous = OFF", NULL, NULL, &pszErrMsg );
        else if (EQUAL(pszSqliteSync, "NORMAL") || EQUAL(pszSqliteSync, "1"))
            rc = sqlite3_exec( hDB, "PRAGMA synchronous = NORMAL", NULL, NULL, &pszErrMsg );
        else if (EQUAL(pszSqliteSync, "ON") || EQUAL(pszSqliteSync, "FULL") ||
            EQUAL(pszSqliteSync, "2") || EQUAL(pszSqliteSync, "TRUE"))
            rc = sqlite3_exec( hDB, "PRAGMA synchronous = FULL", NULL, NULL, &pszErrMsg );
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined, "Unrecognized value for OGR_SQLITE_SYNCHRONOUS : %s",
                      pszSqliteSync);
            rc = SQLITE_OK;
        }

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to create view geom_cols_ref_sys: %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            return FALSE;
        }
    }

    CPLHashSet* hSet = CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, CPLFree);

/* -------------------------------------------------------------------- */
/*      If we have a GEOMETRY_COLUMNS tables, initialize on the basis   */
/*      of that.                                                        */
/* -------------------------------------------------------------------- */
    char **papszResult;
    int nRowCount, iRow, nColCount;

    rc = sqlite3_get_table( 
        hDB,
        "SELECT f_table_name, f_geometry_column, geometry_type, coord_dimension, geometry_format, srid"
        " FROM geometry_columns",
        &papszResult, &nRowCount, &nColCount, &pszErrMsg );

    if( rc == SQLITE_OK )
    {
        CPLDebug("SQLITE", "OGR style SQLite DB found !");
    
        bHaveGeometryColumns = TRUE;

        for( iRow = 0; iRow < nRowCount; iRow++ )
        {
            char **papszRow = papszResult + iRow * 6 + 6;
            OGRwkbGeometryType eGeomType = wkbUnknown;
            int nSRID = 0;

            if (papszRow[0] == NULL ||
                papszRow[1] == NULL ||
                papszRow[2] == NULL ||
                papszRow[3] == NULL)
                continue;

            eGeomType = (OGRwkbGeometryType) atoi(papszRow[2]);

            if( atoi(papszRow[3]) > 2 )
                eGeomType = (OGRwkbGeometryType) (((int)eGeomType) | wkb25DBit);

            if( papszRow[5] != NULL )
                nSRID = atoi(papszRow[5]);

            OpenTable( papszRow[0], papszRow[1], eGeomType, papszRow[4],
                       FetchSRS( nSRID ) );
                       
            if (bListAllTables)
                CPLHashSetInsert(hSet, CPLStrdup(papszRow[0]));
        }

        sqlite3_free_table(papszResult);

        if (bListAllTables)
            goto all_tables;
            
        CPLHashSetDestroy(hSet);
        
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we can deal with SpatiaLite database.                 */
/* -------------------------------------------------------------------- */
    sqlite3_free( pszErrMsg );
    rc = sqlite3_get_table( hDB,
                            "SELECT f_table_name, f_geometry_column, "
                            "type, coord_dimension, srid, "
                            "spatial_index_enabled FROM geometry_columns",
                            &papszResult, &nRowCount, 
                            &nColCount, &pszErrMsg );

    if ( rc == SQLITE_OK )
    {
        
        bIsSpatiaLite = TRUE;
        bHaveGeometryColumns = TRUE;

        int bSpatialiteReadOnly = TRUE;
        int iSpatialiteVersion = -1;

        /* Only enables write-mode if linked against SpatiaLite */
        if( bSpatialiteLoaded == TRUE )
        {
            iSpatialiteVersion = OGRSQLiteGetSpatialiteVersionNumber();
            bSpatialiteReadOnly = FALSE;
        }

        if (bSpatialiteReadOnly && bUpdate)
        {
            CPLDebug("SQLITE", "SpatiaLite-style SQLite DB found, but updating tables disabled because no linking against spatialite library !");
        }
        else
        {
            CPLDebug("SQLITE", "SpatiaLite-style SQLite DB found !");
        }

        /*
        / SpatiaLite v.2.4.0 (or any subsequent) is required
        / to support 2.5D: if an obsolete version of the library
        / is found we'll unconditionally activate 2D casting mode
        */
        int bForce2D = FALSE;
        iSpatialiteVersion = OGRSQLiteGetSpatialiteVersionNumber();
        if ( iSpatialiteVersion < 24)
            bForce2D = TRUE;

        for ( iRow = 0; iRow < nRowCount; iRow++ )
        {
            char **papszRow = papszResult + iRow * 6 + 6;
            OGRwkbGeometryType eGeomType;
            int nSRID = 0;
            int bHasM = FALSE;
            int bHasSpatialIndex = FALSE;

            if (papszRow[0] == NULL ||
                papszRow[1] == NULL ||
                papszRow[2] == NULL ||
                papszRow[3] == NULL)
                continue;

            eGeomType = OGRFromOGCGeomType(papszRow[2]);

            if( strcmp ( papszRow[3], "XYZ" ) == 0 || 
                strcmp ( papszRow[3], "XYZM" ) == 0 || 
                strcmp ( papszRow[3], "3" ) == 0) // SpatiaLite's own 3D geometries 
                eGeomType = (OGRwkbGeometryType) (((int)eGeomType) | wkb25DBit);

            if( strcmp ( papszRow[3], "XYM" ) == 0 || 
                strcmp ( papszRow[3], "XYZM" ) == 0 ) // M coordinate declared 
                bHasM = TRUE;


            if( papszRow[4] != NULL )
                nSRID = atoi(papszRow[4]);

            /* Only look for presence of a spatial index if linked against SpatiaLite */
            if( bSpatialiteLoaded && papszRow[5] != NULL )
                bHasSpatialIndex = atoi(papszRow[5]);

            OpenTable( papszRow[0], papszRow[1], eGeomType, "SpatiaLite",
                       FetchSRS( nSRID ), nSRID, bHasSpatialIndex, bHasM, 
                       bSpatialiteReadOnly, bSpatialiteLoaded,
                       iSpatialiteVersion, bForce2D );
                       
            if (bListAllTables)
                CPLHashSetInsert(hSet, CPLStrdup(papszRow[0]));
        }

        sqlite3_free_table(papszResult);

/* -------------------------------------------------------------------- */
/*      Detect VirtualShape layers                                      */
/* -------------------------------------------------------------------- */
#ifdef HAVE_SPATIALITE
        if (bSpatialiteLoaded)
        {
            rc = sqlite3_get_table( hDB,
                                "SELECT name FROM sqlite_master WHERE sql LIKE 'CREATE VIRTUAL TABLE % USING %VirtualShape%'",
                                &papszResult, &nRowCount, 
                                &nColCount, &pszErrMsg );

            if ( rc == SQLITE_OK )
            {
                for( iRow = 0; iRow < nRowCount; iRow++ )
                {
                    OpenTable( papszResult[iRow+1] );
                    
                    if (bListAllTables)
                        CPLHashSetInsert(hSet, CPLStrdup(papszResult[iRow+1]));
                }
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                        "Unable to fetch list of tables: %s", 
                        pszErrMsg );
                sqlite3_free( pszErrMsg );
            }

            sqlite3_free_table(papszResult);
        }
#endif

/* -------------------------------------------------------------------- */
/*      Detect spatial views                                            */
/* -------------------------------------------------------------------- */
        rc = sqlite3_get_table( hDB,
                                "SELECT view_name, view_geometry, view_rowid, f_table_name, f_geometry_column FROM views_geometry_columns",
                                &papszResult, &nRowCount,
                                &nColCount, &pszErrMsg );
        if ( rc == SQLITE_OK )
        {
            for( iRow = 0; iRow < nRowCount; iRow++ )
            {
                char **papszRow = papszResult + iRow * 5 + 5;
                const char* pszViewName = papszRow[0];
                const char* pszViewGeometry = papszRow[1];
                const char* pszViewRowid = papszRow[2];
                const char* pszTableName = papszRow[3];
                const char* pszGeometryColumn = papszRow[4];

                if (pszViewName == NULL ||
                    pszViewGeometry == NULL ||
                    pszViewRowid == NULL ||
                    pszTableName == NULL ||
                    pszGeometryColumn == NULL)
                    continue;

                OpenView( pszViewName, pszViewGeometry, pszViewRowid,
                          pszTableName, pszGeometryColumn, bSpatialiteLoaded );

                if (bListAllTables)
                    CPLHashSetInsert(hSet, CPLStrdup(pszViewName));
            }
            sqlite3_free_table(papszResult);
        }


        if (bListAllTables)
            goto all_tables;

        CPLHashSetDestroy(hSet);
        
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise our final resort is to return all tables and views    */
/*      as non-spatial tables.                                          */
/* -------------------------------------------------------------------- */
    sqlite3_free( pszErrMsg );
    
all_tables:
    rc = sqlite3_get_table( hDB,
                            "SELECT name FROM sqlite_master "
                            "WHERE type IN ('table','view') "
                            "UNION ALL "
                            "SELECT name FROM sqlite_temp_master "
                            "WHERE type IN ('table','view') "
                            "ORDER BY 1",
                            &papszResult, &nRowCount, 
                            &nColCount, &pszErrMsg );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to fetch list of tables: %s", 
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        CPLHashSetDestroy(hSet);
        return FALSE;
    }
    
    for( iRow = 0; iRow < nRowCount; iRow++ )
    {
        if (CPLHashSetLookup(hSet, papszResult[iRow+1]) == NULL)
            OpenTable( papszResult[iRow+1] );
    }
    
    sqlite3_free_table(papszResult);
    CPLHashSetDestroy(hSet);

    return TRUE;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRSQLiteDataSource::OpenTable( const char *pszNewName, 
                                    const char *pszGeomCol,
                                    OGRwkbGeometryType eGeomType,
                                    const char *pszGeomFormat,
                                    OGRSpatialReference *poSRS, int nSRID,
                                    int bHasSpatialIndex, int bHasM, 
                                    int bSpatialiteReadOnly,
                                    int bSpatialiteLoaded,
                                    int iSpatialiteVersion,
                                    int bForce2D )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteTableLayer  *poLayer;

    poLayer = new OGRSQLiteTableLayer( this );

    if( poLayer->Initialize( pszNewName, pszGeomCol, 
                             eGeomType, pszGeomFormat,
                             poSRS, nSRID, bHasSpatialIndex, 
                             bHasM, bSpatialiteReadOnly,
                             bSpatialiteLoaded,
                             iSpatialiteVersion ) != CE_None )
    {
        delete poLayer;
        return FALSE;
    }
    poLayer->SetSpatialite2D ( bForce2D );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRSQLiteLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRSQLiteLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;
    
    return TRUE;
}

/************************************************************************/
/*                             OpenView()                               */
/************************************************************************/

int OGRSQLiteDataSource::OpenView( const char *pszViewName,
                                   const char *pszViewGeometry,
                                   const char *pszViewRowid,
                                   const char *pszTableName,
                                   const char *pszGeometryColumn,
                                   int bSpatialiteLoaded)

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteViewLayer  *poLayer;

    poLayer = new OGRSQLiteViewLayer( this );

    if( poLayer->Initialize( pszViewName, pszViewGeometry,
                             pszViewRowid, pszTableName, pszGeometryColumn,
                             bSpatialiteLoaded ) != CE_None )
    {
        delete poLayer;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRSQLiteLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRSQLiteLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return bUpdate;
    else if( EQUAL(pszCap,ODsCDeleteLayer) )
        return bUpdate;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSQLiteDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

static const char* apszSpatialiteFuncs[] =
{
    "InitSpatialMetaData",
    "AddGeometryColumn",
    "RecoverGeometryColumn",
    "DiscardGeometryColumn",
    "CreateSpatialIndex",
    "CreateMbrCache",
    "DisableSpatialIndex"
};

OGRLayer * OGRSQLiteDataSource::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
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

        DeleteLayer( pszLayerName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Prepare statement.                                              */
/* -------------------------------------------------------------------- */
    int rc;
    sqlite3_stmt *hSQLStmt = NULL;

    rc = sqlite3_prepare( GetDB(), pszSQLCommand, strlen(pszSQLCommand),
                          &hSQLStmt, NULL );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                "In ExecuteSQL(): sqlite3_prepare(%s):\n  %s", 
                pszSQLCommand, sqlite3_errmsg(GetDB()) );

        if( hSQLStmt != NULL )
        {
            sqlite3_finalize( hSQLStmt );
        }

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
                  pszSQLCommand, sqlite3_errmsg(GetDB()) );
        }
        sqlite3_finalize( hSQLStmt );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Special case for some spatialite functions which must be run    */
/*      only once                                                       */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand,"SELECT ",7) &&
        bIsSpatiaLite && bSpatialiteLoaded )
    {
        unsigned int i;
        for(i=0;i<sizeof(apszSpatialiteFuncs)/
                  sizeof(apszSpatialiteFuncs[0]);i++)
        {
            if( EQUALN(apszSpatialiteFuncs[i], pszSQLCommand + 7,
                       strlen(apszSpatialiteFuncs[i])) )
            {
                if (sqlite3_column_count( hSQLStmt ) == 1 &&
                    sqlite3_column_type( hSQLStmt, 0 ) == SQLITE_INTEGER )
                {
                    int ret = sqlite3_column_int( hSQLStmt, 0 );

                    sqlite3_finalize( hSQLStmt );

                    return new OGRSQLiteSingleFeatureLayer
                                        ( apszSpatialiteFuncs[i], ret );
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create layer.                                                   */
/* -------------------------------------------------------------------- */
    OGRSQLiteSelectLayer *poLayer = NULL;
        
    CPLString osSQL = pszSQLCommand;
    poLayer = new OGRSQLiteSelectLayer( this, osSQL, hSQLStmt );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );
    
    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRSQLiteDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRSQLiteDataSource::CreateLayer( const char * pszLayerNameIn,
                                  OGRSpatialReference *poSRS,
                                  OGRwkbGeometryType eType,
                                  char ** papszOptions )

{
    char                *pszLayerName;
    int                  bForce2D = FALSE;
    const char          *pszGeomFormat;

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  pszName, pszLayerNameIn );

        return NULL;
    }

    if( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) )
        pszLayerName = LaunderName( pszLayerNameIn );
    else
        pszLayerName = CPLStrdup( pszLayerNameIn );

    CPLString osEscapedLayerName = OGRSQLiteEscape(pszLayerName);
    const char* pszEscapedLayerName = osEscapedLayerName.c_str();
    
    pszGeomFormat = CSLFetchNameValue( papszOptions, "FORMAT" );
    if( pszGeomFormat == NULL )
    {
        if ( !bIsSpatiaLite )
            pszGeomFormat = "WKB";
        else
            pszGeomFormat = "SpatiaLite";
    }

    if( !EQUAL(pszGeomFormat,"WKT") 
        && !EQUAL(pszGeomFormat,"WKB")
        && !EQUAL(pszGeomFormat, "SpatiaLite") )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "FORMAT=%s not recognised or supported.", 
                  pszGeomFormat );
        return NULL;
    }

    if (bIsSpatiaLite && !EQUAL(pszGeomFormat, "SpatiaLite") )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "FORMAT=%s not support on a SpatiaLite enabled database.",
                  pszGeomFormat );
        return NULL;
    }
    if (bIsSpatiaLite && !bSpatialiteLoaded)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Creating layers on a SpatiaLite enabled database, "
                  "without Spatialite extensions loaded, is not supported." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    int iLayer;

    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetLayerDefn()->GetName()) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                DeleteLayer( pszLayerName );
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszLayerName );
                CPLFree( pszLayerName );
                return NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding to the srs table if needed.                              */
/* -------------------------------------------------------------------- */
    int nSRSId = -1;

    if( poSRS != NULL )
        nSRSId = FetchSRSId( poSRS );

/* -------------------------------------------------------------------- */
/*      Create a basic table with the FID.  Also include the            */
/*      geometry if this is not a PostGIS enabled table.                */
/* -------------------------------------------------------------------- */
    int rc;
    char *pszErrMsg;
    const char *pszGeomCol = NULL;
    CPLString osCommand;

    if( eType == wkbNone )
        osCommand.Printf( 
            "CREATE TABLE '%s' ( OGC_FID INTEGER PRIMARY KEY )", 
            pszEscapedLayerName );
    else
    {
        if( EQUAL(pszGeomFormat,"WKT") )
        {
            pszGeomCol = "WKT_GEOMETRY";
            osCommand.Printf(
                "CREATE TABLE '%s' ( "
                "  OGC_FID INTEGER PRIMARY KEY,"
                "  %s VARCHAR )", 
                pszEscapedLayerName, pszGeomCol );
        }
        else
        {
            pszGeomCol = "GEOMETRY";

            /* Only if was created as a SpatiaLite DB */
            if ( bIsSpatiaLite )
            {
                /* 
                / SpatiaLite full support: we must create the 
                / Geometry in a second time using AddGeometryColumn()
                /
                / IMPORTANT NOTICE: on SpatiaLite any attempt aimed
                / to directly creating some Geometry column 
                / [by-passing AddGeometryColumn() as absolutely required]
                / will severely [and irremediably] corrupt the DB !!!
                */
                osCommand.Printf( "CREATE TABLE '%s' ( "
                                  "  OGC_FID INTEGER PRIMARY KEY)",
                                  pszLayerName);
            }
            else
            {
                osCommand.Printf( "CREATE TABLE '%s' ( "
                                  "  OGC_FID INTEGER PRIMARY KEY,"
                                  "  %s BLOB )", 
                                  pszLayerName, pszGeomCol );
            }
        }
    }

#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif

    rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create table %s: %s",
                  pszLayerName, pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Eventually we should be adding this table to a table of         */
/*      "geometric layers", capturing the WKT projection, and           */
/*      perhaps some other housekeeping.                                */
/* -------------------------------------------------------------------- */
    if( bHaveGeometryColumns && eType != wkbNone )
    {
        int nCoordDim;

        /* Sometimes there is an old cruft entry in the geometry_columns
         * table if things were not properly cleaned up before.  We make
         * an effort to clean out such cruft.
         */
        osCommand.Printf(
            "DELETE FROM geometry_columns WHERE f_table_name = '%s'", 
            pszEscapedLayerName );
                 
#ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif

        rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            sqlite3_free( pszErrMsg );
            return FALSE;
        }
        
        if( eType == wkbFlatten(eType) )
            nCoordDim = 2;
        else
            nCoordDim = 3;
        
        if ( bIsSpatiaLite )
        {
            /*
            / SpatiaLite full support: calling AddGeometryColumn()
            /
            / IMPORTANT NOTICE: on SpatiaLite any attempt aimed
            / to directly INSERT a row into GEOMETRY_COLUMNS
            / [by-passing AddGeometryColumn() as absolutely required]
            / will severely [and irremediably] corrupt the DB !!!
            */
            const char *pszType = OGRToOGCGeomType(eType);
            if (pszType[0] == '\0')
                pszType = "GEOMETRY";

            /*
            / SpatiaLite v.2.4.0 (or any subsequent) is required
            / to support 2.5D: if an obsolete version of the library
            / is found we'll unconditionally activate 2D casting mode
            */
            int iSpatialiteVersion = OGRSQLiteGetSpatialiteVersionNumber();
            if ( iSpatialiteVersion < 24 && nCoordDim == 3 )
            {
                CPLDebug("SQLITE", "Spatialite < 2.4.0 --> 2.5D geometry not supported. Casting to 2D");
                nCoordDim = 2;
                bForce2D = TRUE;
            }

            osCommand.Printf( "SELECT AddGeometryColumn("
                              "'%s', '%s', %d, '%s', %d)",
                              pszLayerName, pszGeomCol, nSRSId,
                              pszType, nCoordDim );
        }
        else
        {
            if( nSRSId > 0 )
            {
                osCommand.Printf(
                    "INSERT INTO geometry_columns "
                    "(f_table_name, f_geometry_column, geometry_format, "
                    "geometry_type, coord_dimension, srid) VALUES "
                    "('%s','%s','%s', %d, %d, %d)", 
                    pszEscapedLayerName, pszGeomCol, pszGeomFormat,
                    (int) wkbFlatten(eType), nCoordDim, nSRSId );
            }
            else
            {
                osCommand.Printf(
                    "INSERT INTO geometry_columns "
                    "(f_table_name, f_geometry_column, geometry_format, "
                    "geometry_type, coord_dimension) VALUES "
                    "('%s','%s','%s', %d, %d)",
                    pszEscapedLayerName, pszGeomCol, pszGeomFormat,
                    (int) wkbFlatten(eType), nCoordDim );
            }
        }

#ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif

        rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to add %s table to geometry_columns:\n%s",
                      pszLayerName, pszErrMsg );
            sqlite3_free( pszErrMsg );
            return FALSE;
        }

/* -------------------------------------------------------------------- */
/*      Create the spatial index.                                       */
/*                                                                      */
/*      We're doing this before we add geometry and record to the table */
/*      so this may not be exactly the best way to do it.               */
/* -------------------------------------------------------------------- */

        const char* pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
        if ( pszSI != NULL && CSLTestBoolean(pszSI) &&
             (bIsSpatiaLite || EQUAL(pszGeomFormat, "SpatiaLite")) && !bSpatialiteLoaded )
        {
            CPLError( CE_Warning, CPLE_OpenFailed,
                    "Cannot create a spatial index when Spatialite extensions are not loaded." );
        }

#ifdef HAVE_SPATIALITE
        /* Only if linked against SpatiaLite and the datasource was created as a SpatiaLite DB */
        if ( bIsSpatiaLite && bSpatialiteLoaded )
#else
        if ( 0 )
#endif
        {
            if( pszSI == NULL || CSLTestBoolean(pszSI) )
            {
                osCommand.Printf("SELECT CreateSpatialIndex('%s', '%s')",
                                 pszEscapedLayerName, pszGeomCol);

                rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
                if( rc != SQLITE_OK )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                            "Unable to create spatial index:\n%s", pszErrMsg );
                    sqlite3_free( pszErrMsg );
                    return FALSE;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteTableLayer     *poLayer;

    poLayer = new OGRSQLiteTableLayer( this );

    if ( poLayer->Initialize( pszLayerName, pszGeomCol, eType, pszGeomFormat,
                         FetchSRS(nSRSId), nSRSId ) != CE_None )
    {
        delete poLayer;
        CPLFree( pszLayerName );
        return NULL;
    }

    poLayer->SetLaunderFlag( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) );
    if ( CSLFetchBoolean(papszOptions,"COMPRESS_GEOM",FALSE) )
        poLayer->SetUseCompressGeom( TRUE );
    poLayer->SetSpatialite2D ( bForce2D );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRSQLiteLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRSQLiteLayer *) * (nLayers+1) );
    
    papoLayers[nLayers++] = poLayer;

    CPLFree( pszLayerName );

    return poLayer;
}

/************************************************************************/
/*                            LaunderName()                             */
/************************************************************************/

char *OGRSQLiteDataSource::LaunderName( const char *pszSrcName )

{
    char    *pszSafeName = CPLStrdup( pszSrcName );
    int     i;

    for( i = 0; pszSafeName[i] != '\0'; i++ )
    {
        pszSafeName[i] = (char) tolower( pszSafeName[i] );
        if( pszSafeName[i] == '\'' || pszSafeName[i] == '-' || pszSafeName[i] == '#' )
            pszSafeName[i] = '_';
    }

    return pszSafeName;
}

/************************************************************************/
/*                          OGRSQLiteEscape()                           */
/************************************************************************/

CPLString OGRSQLiteEscape( const char *pszSrcName )
{
    CPLString osVal;
    for( int i = 0; pszSrcName[i] != '\0'; i++ )
    {
        if ( pszSrcName[i] == '\'' )
            osVal += '\'';
        osVal += pszSrcName[i];
    }
    return osVal;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

void OGRSQLiteDataSource::DeleteLayer( const char *pszLayerName )

{
    int iLayer;

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "Layer %s cannot be deleted.\n",
                  pszName, pszLayerName );

        return;
    }

/* -------------------------------------------------------------------- */
/*      Try to find layer.                                              */
/* -------------------------------------------------------------------- */
    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetLayerDefn()->GetName()) )
            break;
    }

    if( iLayer == nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to delete layer '%s', but this layer is not known to OGR.", 
                  pszLayerName );
        return;
    }

    DeleteLayer(iLayer);
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRSQLiteDataSource::DeleteLayer(int iLayer)
{
    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %d not in legal range of 0 to %d.",
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

    CPLString osLayerName = GetLayer(iLayer)->GetName();
    CPLString osGeometryColumn = GetLayer(iLayer)->GetGeometryColumn();

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLDebug( "OGR_SQLITE", "DeleteLayer(%s)", osLayerName.c_str() );

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1, 
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */
    int rc;
    char *pszErrMsg;

    CPLString osEscapedLayerName = OGRSQLiteEscape(osLayerName);
    const char* pszEscapedLayerName = osEscapedLayerName.c_str();
    const char* pszGeometryColumn = osGeometryColumn.size() ? osGeometryColumn.c_str() : NULL;

    rc = sqlite3_exec( hDB, CPLSPrintf( "DROP TABLE '%s'", pszEscapedLayerName ),
                       NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to drop table %s: %s",
                  osLayerName.c_str(), pszErrMsg );
        sqlite3_free( pszErrMsg );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Drop from geometry_columns table.                               */
/* -------------------------------------------------------------------- */
    if( bHaveGeometryColumns )
    {
        CPLString osCommand;

        osCommand.Printf( 
            "DELETE FROM geometry_columns WHERE f_table_name = '%s'",
            pszEscapedLayerName );
        
        rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Removal from geometry_columns failed.\n%s: %s", 
                      osCommand.c_str(), pszErrMsg );
            sqlite3_free( pszErrMsg );
            return OGRERR_FAILURE;
        }

/* -------------------------------------------------------------------- */
/*      Drop spatialite spatial index tables                            */
/* -------------------------------------------------------------------- */
        if( bIsSpatiaLite && pszGeometryColumn )
        {
            osCommand.Printf( "DROP TABLE idx_%s_%s", pszEscapedLayerName,
                              pszGeometryColumn);
            rc = sqlite3_exec( hDB, osCommand, NULL, NULL, NULL );

            osCommand.Printf( "DROP TABLE idx_%s_%s_node", pszEscapedLayerName,
                              pszGeometryColumn);
            rc = sqlite3_exec( hDB, osCommand, NULL, NULL, NULL );

            osCommand.Printf( "DROP TABLE idx_%s_%s_parent", pszEscapedLayerName,
                              pszGeometryColumn);
            rc = sqlite3_exec( hDB, osCommand, NULL, NULL, NULL );

            osCommand.Printf( "DROP TABLE idx_%s_%s_rowid", pszEscapedLayerName,
                              pszGeometryColumn);
            rc = sqlite3_exec( hDB, osCommand, NULL, NULL, NULL );
        }
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                        SoftStartTransaction()                        */
/*                                                                      */
/*      Create a transaction scope.  If we already have a               */
/*      transaction active this isn't a real transaction, but just      */
/*      an increment to the scope count.                                */
/************************************************************************/

OGRErr OGRSQLiteDataSource::SoftStartTransaction()

{
    nSoftTransactionLevel++;

    if( nSoftTransactionLevel == 1 )
    {
        int rc;
        char *pszErrMsg;
        
#ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "BEGIN Transaction" );
#endif

        rc = sqlite3_exec( hDB, "BEGIN", NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            nSoftTransactionLevel--;
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "BEGIN transaction failed: %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            return OGRERR_FAILURE;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             SoftCommit()                             */
/*                                                                      */
/*      Commit the current transaction if we are at the outer           */
/*      scope.                                                          */
/************************************************************************/

OGRErr OGRSQLiteDataSource::SoftCommit()

{
    if( nSoftTransactionLevel <= 0 )
    {
        CPLDebug( "OGR_SQLITE", "SoftCommit() with no transaction active." );
        return OGRERR_FAILURE;
    }

    nSoftTransactionLevel--;

    if( nSoftTransactionLevel == 0 )
    {
        int rc;
        char *pszErrMsg;
        
#ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "COMMIT Transaction" );
#endif

        rc = sqlite3_exec( hDB, "COMMIT", NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "COMMIT transaction failed: %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            return OGRERR_FAILURE;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            SoftRollback()                            */
/*                                                                      */
/*      Force a rollback of the current transaction if there is one,    */
/*      even if we are nested several levels deep.                      */
/************************************************************************/

OGRErr OGRSQLiteDataSource::SoftRollback()

{
    if( nSoftTransactionLevel <= 0 )
    {
        CPLDebug( "OGR_SQLITE", "SoftRollback() with no transaction active." );
        return OGRERR_FAILURE;
    }

    nSoftTransactionLevel = 0;

    int rc;
    char *pszErrMsg;
    
#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "ROLLBACK Transaction" );
#endif

    rc = sqlite3_exec( hDB, "ROLLBACK", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "ROLLBACK transaction failed: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                        FlushSoftTransaction()                        */
/*                                                                      */
/*      Force the unwinding of any active transaction, and it's         */
/*      commit.                                                         */
/************************************************************************/

OGRErr OGRSQLiteDataSource::FlushSoftTransaction()

{
    if( nSoftTransactionLevel <= 0 )
        return OGRERR_NONE;

    nSoftTransactionLevel = 1;

    return SoftCommit();
}

/************************************************************************/
/*                            DetectSRSWktColumn()                            */
/************************************************************************/

int OGRSQLiteDataSource::DetectSRSWktColumn()
{
    int bHasSrsWkt = FALSE;

/* testing for SRS_WKT column presence */
    char **papszResult;
    int nRowCount, nColCount;
    char *pszErrMsg = NULL;
    int rc = sqlite3_get_table( hDB, "PRAGMA table_info(spatial_ref_sys)",
                            &papszResult, &nRowCount, &nColCount,
                            &pszErrMsg );

    if( rc == SQLITE_OK )
    {
        int iRow;
        for (iRow = 1; iRow <= nRowCount; iRow++)
        {
            if (EQUAL("srs_wkt",
                        papszResult[(iRow * nColCount) + 1]))
                bHasSrsWkt = TRUE;
        }
        sqlite3_free_table(papszResult);
    }
    else
    {
        sqlite3_free( pszErrMsg );
    }

    return bHasSrsWkt;
}

/************************************************************************/
/*                             FetchSRSId()                             */
/*                                                                      */
/*      Fetch the id corresponding to an SRS, and if not found, add     */
/*      it to the table.                                                */
/************************************************************************/

int OGRSQLiteDataSource::FetchSRSId( OGRSpatialReference * poSRS )

{
    int                 nSRSId = -1;
    const char          *pszAuthorityName, *pszAuthorityCode = NULL;
    CPLString           osCommand;
    char *pszErrMsg;
    int   rc;
    char **papszResult;
    int nRowCount, nColCount;

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
            pszAuthorityCode = oSRS.GetAuthorityCode(NULL);
            if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                oSRS.importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthorityName = oSRS.GetAuthorityName(NULL);
                pszAuthorityCode = oSRS.GetAuthorityCode(NULL);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Check whether the EPSG authority code is already mapped to a    */
/*      SRS ID.                                                         */
/* -------------------------------------------------------------------- */
    if( pszAuthorityName != NULL && strlen(pszAuthorityName) > 0 )
    {
        pszAuthorityCode = oSRS.GetAuthorityCode(NULL);

        if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
        {
            // XXX: We are using case insensitive comparison for "auth_name"
            // values, because there are variety of options exist. By default
            // the driver uses 'EPSG' in upper case, but SpatiaLite extension
            // uses 'epsg' in lower case.
            osCommand.Printf( "SELECT srid FROM spatial_ref_sys WHERE "
                              "auth_name = '%s' COLLATE NOCASE AND auth_srid = '%s'",
                              pszAuthorityName, pszAuthorityCode );

            rc = sqlite3_get_table( hDB, osCommand, &papszResult, 
                                    &nRowCount, &nColCount, &pszErrMsg );
            if( rc != SQLITE_OK )
            {
                /* Retry without COLLATE NOCASE which may not be understood by older sqlite3 */
                sqlite3_free( pszErrMsg );

                osCommand.Printf( "SELECT srid FROM spatial_ref_sys WHERE "
                                  "auth_name = '%s' AND auth_srid = '%s'",
                                  pszAuthorityName, pszAuthorityCode );

                rc = sqlite3_get_table( hDB, osCommand, &papszResult, 
                                        &nRowCount, &nColCount, &pszErrMsg );

                /* Retry in lower case for SpatiaLite */
                if( rc != SQLITE_OK )
                {
                    sqlite3_free( pszErrMsg );
                }
                else if ( nRowCount == 0 &&
                          strcmp(pszAuthorityName, "EPSG") == 0)
                {
                    /* If it's in upper case, look for lower case */
                    sqlite3_free_table(papszResult);

                    osCommand.Printf( "SELECT srid FROM spatial_ref_sys WHERE "
                                      "auth_name = 'epsg' AND auth_srid = '%s'",
                                      pszAuthorityCode );

                    rc = sqlite3_get_table( hDB, osCommand, &papszResult, 
                                            &nRowCount, &nColCount, &pszErrMsg );

                    if( rc != SQLITE_OK )
                    {
                        sqlite3_free( pszErrMsg );
                    }
                }
            }

            if( rc == SQLITE_OK && nRowCount == 1 )
            {
                nSRSId = (papszResult[1] != NULL) ? atoi(papszResult[1]) : -1;
                sqlite3_free_table(papszResult);
                return nSRSId;
            }
            sqlite3_free_table(papszResult);
        }
    }

/* -------------------------------------------------------------------- */
/*      Search for existing record using either WKT definition or       */
/*      PROJ.4 string (SpatiaLite variant).                             */
/* -------------------------------------------------------------------- */
    CPLString   osWKT, osProj4;

/* -------------------------------------------------------------------- */
/*      Translate SRS to WKT.                                           */
/* -------------------------------------------------------------------- */
    char    *pszWKT = NULL;

    if( oSRS.exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLFree(pszWKT);
        return -1;
    }

    osWKT = pszWKT;
    CPLFree( pszWKT );
    pszWKT = NULL;

    if ( !bIsSpatiaLite )
    {
/* -------------------------------------------------------------------- */
/*      Try to find based on the WKT match.                             */
/* -------------------------------------------------------------------- */
        osCommand.Printf( "SELECT srid FROM spatial_ref_sys WHERE srtext = ?");
    }

/* -------------------------------------------------------------------- */
/*      Handle SpatiaLite flavour of the spatial_ref_sys.               */
/* -------------------------------------------------------------------- */
    else
    {
/* -------------------------------------------------------------------- */
/*      Translate SRS to PROJ.4 string.                                 */
/* -------------------------------------------------------------------- */
        char    *pszProj4 = NULL;

        if( oSRS.exportToProj4( &pszProj4 ) != OGRERR_NONE )
        {
            CPLFree(pszProj4);
            return -1;
        }

        osProj4 = pszProj4;
        CPLFree( pszProj4 );
        pszProj4 = NULL;

/* -------------------------------------------------------------------- */
/*      Try to find based on the PROJ.4 match.                          */
/* -------------------------------------------------------------------- */
        osCommand.Printf(
            "SELECT srid FROM spatial_ref_sys WHERE proj4text = ?");
    }

    sqlite3_stmt *hSelectStmt = NULL;
    rc = sqlite3_prepare( hDB, osCommand, -1, &hSelectStmt, NULL );

    if( rc == SQLITE_OK)
        rc = sqlite3_bind_text( hSelectStmt, 1, ( !bIsSpatiaLite ) ? osWKT.c_str() : osProj4.c_str(), -1, SQLITE_STATIC );

    if( rc == SQLITE_OK)
        rc = sqlite3_step( hSelectStmt );

    if (rc == SQLITE_ROW)
    {
        if (sqlite3_column_type( hSelectStmt, 0 ) == SQLITE_INTEGER)
            nSRSId = sqlite3_column_int( hSelectStmt, 0 );
        else
            nSRSId = -1;

        sqlite3_finalize( hSelectStmt );
        return nSRSId;
    }

/* -------------------------------------------------------------------- */
/*      If the command actually failed, then the metadata table is      */
/*      likely missing, so we give up.                                  */
/* -------------------------------------------------------------------- */
    if (rc != SQLITE_DONE && rc != SQLITE_ROW)
    {
        sqlite3_finalize( hSelectStmt );
        return -1;
    }

    sqlite3_finalize( hSelectStmt );

/* -------------------------------------------------------------------- */
/*      If we have an authority code try to assign SRS ID the same      */
/*      as that code.                                                   */
/* -------------------------------------------------------------------- */
    if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
    {
        osCommand.Printf( "SELECT * FROM spatial_ref_sys WHERE auth_srid='%s'",
                          pszAuthorityCode );
        rc = sqlite3_get_table( hDB, osCommand, &papszResult,
                                &nRowCount, &nColCount, &pszErrMsg );
        
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "exec(SELECT '%s' FROM spatial_ref_sys) failed: %s",
                      pszAuthorityCode, pszErrMsg );
            sqlite3_free( pszErrMsg );
        }

/* -------------------------------------------------------------------- */
/*      If there is no SRS ID with such auth_srid, use it as SRS ID.    */
/* -------------------------------------------------------------------- */
        if ( nRowCount < 1 )
            nSRSId = atoi(pszAuthorityCode);
        sqlite3_free_table(papszResult);
    }

/* -------------------------------------------------------------------- */
/*      Otherwise get the current maximum srid in the srs table.        */
/* -------------------------------------------------------------------- */
    if ( nSRSId == -1 )
    {
        rc = sqlite3_get_table( hDB, "SELECT MAX(srid) FROM spatial_ref_sys", 
                                &papszResult, &nRowCount, &nColCount,
                                &pszErrMsg );
        
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "SELECT of the maximum SRS ID failed: %s", pszErrMsg );
            sqlite3_free( pszErrMsg );
            return -1;
        }

        if ( nRowCount < 1 || !papszResult[1] )
            nSRSId = 50000;
        else
            nSRSId = atoi(papszResult[1]) + 1;  // Insert as the next SRS ID
        sqlite3_free_table(papszResult);
    }

/* -------------------------------------------------------------------- */
/*      Try adding the SRS to the SRS table.                            */
/* -------------------------------------------------------------------- */

    const char* apszToInsert[] = { NULL, NULL, NULL, NULL, NULL, NULL };

    if ( !bIsSpatiaLite )
    {
        if( pszAuthorityName != NULL )
        {
            osCommand.Printf(
                "INSERT INTO spatial_ref_sys (srid,srtext,auth_name,auth_srid) "
                "                     VALUES (%d, ?, ?, ?)",
                nSRSId );
            apszToInsert[0] = osWKT.c_str();
            apszToInsert[1] = pszAuthorityName;
            apszToInsert[2] = pszAuthorityCode;
        }
        else
        {
            osCommand.Printf(
                "INSERT INTO spatial_ref_sys (srid,srtext) "
                "                     VALUES (%d, ?)",
                nSRSId );
            apszToInsert[0] = osWKT.c_str();
        }
    }
    else
    {
        int bHasSrsWkt = DetectSRSWktColumn();

        const char  *pszProjCS = oSRS.GetAttrValue("PROJCS");
        if (pszProjCS == NULL)
            pszProjCS = oSRS.GetAttrValue("GEOGCS");

        if( pszAuthorityName != NULL )
        {
            if ( pszProjCS )
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, ref_sys_name, proj4text%s) "
                    "VALUES (%d, ?, ?, ?, ?%s)",
                    bHasSrsWkt ? ", srs_wkt" : "",
                    nSRSId,
                    bHasSrsWkt ? ", ?" : "");
                apszToInsert[0] = pszAuthorityName;
                apszToInsert[1] = pszAuthorityCode;
                apszToInsert[2] = pszProjCS;
                apszToInsert[3] = osProj4.c_str();
                apszToInsert[4] = bHasSrsWkt ? osWKT.c_str() : NULL;
            }
            else
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, proj4text%s) "
                    "VALUES (%d, ?, ?, ?%s)",
                    bHasSrsWkt ? ", srs_wkt" : "",
                    nSRSId,
                    bHasSrsWkt ? ", ?" : "");
                apszToInsert[0] = pszAuthorityName;
                apszToInsert[1] = pszAuthorityCode;
                apszToInsert[2] = osProj4.c_str();
                apszToInsert[3] = bHasSrsWkt ? osWKT.c_str() : NULL;
            }
        }
        else
        {
            /* SpatiaLite spatial_ref_sys auth_name and auth_srid columns must be NOT NULL */
            /* so insert within a fake OGR "authority" */
            if ( pszProjCS )
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, ref_sys_name, proj4text%s) VALUES (%d, 'OGR', %d, ?, ?%s)",
                    bHasSrsWkt ? ", srs_wkt" : "",
                    nSRSId, nSRSId,
                    bHasSrsWkt ? ", ?" : "");
                apszToInsert[0] = pszProjCS;
                apszToInsert[1] = osProj4.c_str();
                apszToInsert[2] = bHasSrsWkt ? osWKT.c_str() : NULL;
            }
            else
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, proj4text%s) VALUES (%d, 'OGR', %d, ?%s)",
                    bHasSrsWkt ? ", srs_wkt" : "",
                    nSRSId, nSRSId,
                    bHasSrsWkt ? ", ?" : "");
                apszToInsert[0] = osProj4.c_str();
                apszToInsert[1] = bHasSrsWkt ? osWKT.c_str() : NULL;
            }
        }
    }

    sqlite3_stmt *hInsertStmt = NULL;
    rc = sqlite3_prepare( hDB, osCommand, -1, &hInsertStmt, NULL );

    int i;
    for(i=0;apszToInsert[i]!=NULL;i++)
    {
        if( rc == SQLITE_OK)
            rc = sqlite3_bind_text( hInsertStmt, i+1, apszToInsert[i], -1, SQLITE_STATIC );
    }

    if( rc == SQLITE_OK)
        rc = sqlite3_step( hInsertStmt );

    if( rc != SQLITE_OK && rc != SQLITE_DONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to insert SRID (%s): %s",
                  osCommand.c_str(), sqlite3_errmsg(hDB) );

        sqlite3_finalize( hInsertStmt );
        return FALSE;
    }

    sqlite3_finalize( hInsertStmt );

    return nSRSId;
}

/************************************************************************/
/*                              FetchSRS()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

OGRSpatialReference *OGRSQLiteDataSource::FetchSRS( int nId )

{
    if( nId <= 0 )
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
    char *pszErrMsg;
    int   rc;
    char **papszResult;
    int nRowCount, nColCount;
    CPLString osCommand;
    OGRSpatialReference *poSRS = NULL;

    osCommand.Printf( "SELECT srtext FROM spatial_ref_sys WHERE srid = %d",
                      nId );
    rc = sqlite3_get_table( hDB, osCommand, 
                            &papszResult, &nRowCount, &nColCount, &pszErrMsg );

    if ( rc == SQLITE_OK )
    {
        if( nRowCount < 1 )
        {
            sqlite3_free_table(papszResult);
            return NULL;
        }

        char** papszRow = papszResult + nColCount;
        if (papszRow[0] != NULL)
        {
            CPLString osWKT = papszRow[0];

/* -------------------------------------------------------------------- */
/*      Translate into a spatial reference.                             */
/* -------------------------------------------------------------------- */
            char *pszWKT = (char *) osWKT.c_str();

            poSRS = new OGRSpatialReference();
            if( poSRS->importFromWkt( &pszWKT ) != OGRERR_NONE )
            {
                delete poSRS;
                poSRS = NULL;
            }
        }

        sqlite3_free_table(papszResult);
    }

/* -------------------------------------------------------------------- */
/*      Next try SpatiaLite flavour. SpatiaLite uses PROJ.4 strings     */
/*      in 'proj4text' column instead of WKT in 'srtext'. Note: recent  */
/*      versions of spatialite have a srs_wkt column too                */
/* -------------------------------------------------------------------- */
    else
    {
        sqlite3_free( pszErrMsg );
        pszErrMsg = NULL;

        int bHasSrsWkt = DetectSRSWktColumn();

        osCommand.Printf(
            "SELECT proj4text, auth_name, auth_srid%s FROM spatial_ref_sys WHERE srid = %d",
            bHasSrsWkt ? ", srs_wkt" : "", nId );
        rc = sqlite3_get_table( hDB, osCommand, 
                                &papszResult, &nRowCount,
                                &nColCount, &pszErrMsg );
        if ( rc == SQLITE_OK )
        {
            if( nRowCount < 1 )
            {
                sqlite3_free_table(papszResult);
                return NULL;
            }

/* -------------------------------------------------------------------- */
/*      Translate into a spatial reference.                             */
/* -------------------------------------------------------------------- */
            char** papszRow = papszResult + nColCount;

            const char* pszProj4Text = papszRow[0];
            const char* pszAuthName = papszRow[1];
            int nAuthSRID = (papszRow[2] != NULL) ? atoi(papszRow[2]) : 0;
            char* pszWKT = (bHasSrsWkt) ? (char*) papszRow[3] : NULL;

            poSRS = new OGRSpatialReference();

            /* Try first from EPSG code */
            if (pszAuthName != NULL &&
                EQUAL(pszAuthName, "EPSG") &&
                poSRS->importFromEPSG( nAuthSRID ) == OGRERR_NONE)
            {
                /* Do nothing */
            }
            /* Then from WKT string */
            else if( pszWKT != NULL &&
                     poSRS->importFromWkt( &pszWKT ) == OGRERR_NONE )
            {
                /* Do nothing */
            }
            /* Finally from Proj4 string */
            else if( pszProj4Text != NULL &&
                     poSRS->importFromProj4( pszProj4Text ) == OGRERR_NONE )
            {
                /* Do nothing */
            }
            else
            {
                delete poSRS;
                poSRS = NULL;
            }

            sqlite3_free_table(papszResult);
        }

/* -------------------------------------------------------------------- */
/*      No success, report an error.                                    */
/* -------------------------------------------------------------------- */
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "%s: %s", osCommand.c_str(), pszErrMsg );
            sqlite3_free( pszErrMsg );
            return NULL;
        }
    }

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
