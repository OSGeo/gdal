/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 *
 * Contributor: Alessandro Furieri, a.furieri@lqt.it
 * Portions of this module properly supporting SpatiaLite DB creation
 * Developed for Faunalia ( http://www.faunalia.it) with funding from 
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
#include "cpl_csv.h"

#ifdef HAVE_SPATIALITE
#include "spatialite.h"
#endif

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ~OGRSQLiteDriver()                        */
/************************************************************************/

OGRSQLiteDriver::~OGRSQLiteDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRSQLiteDriver::GetName()

{
    return "SQLite";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRSQLiteDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
/* -------------------------------------------------------------------- */
/*      Verify that the target is a real file, and has an               */
/*      appropriate magic string at the beginning.                      */
/* -------------------------------------------------------------------- */
    char szHeader[16];

#ifdef HAVE_SQLITE_VFS
    VSILFILE *fpDB;
    fpDB = VSIFOpenL( pszFilename, "rb" );
    if( fpDB == NULL )
        return NULL;
    
    if( VSIFReadL( szHeader, 1, 16, fpDB ) != 16 )
        memset( szHeader, 0, 16 );
    
    VSIFCloseL( fpDB );
#else
    FILE *fpDB;
    fpDB = VSIFOpen( pszFilename, "rb" );
    if( fpDB == NULL )
        return NULL;

    if( VSIFRead( szHeader, 1, 16, fpDB ) != 16 )
        memset( szHeader, 0, 16 );

    VSIFClose( fpDB );
#endif
    
    if( strncmp( szHeader, "SQLite format 3", 15 ) != 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      We think this is really an SQLite database, go ahead and try    */
/*      and open it.                                                    */
/* -------------------------------------------------------------------- */
    OGRSQLiteDataSource     *poDS;

    poDS = new OGRSQLiteDataSource();

    if( !poDS->Open( pszFilename, bUpdate ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRSQLiteDriver::CreateDataSource( const char * pszName,
                                                  char **papszOptions )

{
/* -------------------------------------------------------------------- */
/*      First, ensure there isn't any such file yet.                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszName, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "It seems a file system object called '%s' already exists.",
                  pszName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check that spatialite extensions are loaded if required to      */
/*      create a spatialite database                                    */
/* -------------------------------------------------------------------- */
    int bSpatialite = CSLFetchBoolean( papszOptions, "SPATIALITE", FALSE );

    if (bSpatialite == TRUE)
    {
#ifdef HAVE_SPATIALITE
        int bSpatialiteLoaded = OGRSQLiteInitSpatialite();
        if (!bSpatialiteLoaded)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "Creating a Spatialite database, but Spatialite extensions are not loaded." );
            return NULL;
        }
#else
        CPLError( CE_Failure, CPLE_NotSupported, 
            "OGR was built without libspatialite support\n"
            "... sorry, creating/writing any SpatiaLite DB is unsupported\n" );
 
        return NULL;
#endif
	}

/* -------------------------------------------------------------------- */
/*      Create the database file.                                       */
/* -------------------------------------------------------------------- */
    sqlite3             *hDB;
    int rc;

    hDB = NULL;
#ifdef HAVE_SQLITE_VFS
    sqlite3_vfs* pMyVFS = NULL;
    int bUseOGRVFS = CSLTestBoolean(CPLGetConfigOption("SQLITE_USE_OGR_VFS", "NO"));
    if (bUseOGRVFS || strncmp(pszName, "/vsi", 4) == 0)
    {
        pMyVFS = OGRSQLiteCreateVFS();
        sqlite3_vfs_register(pMyVFS, 0);
        rc = sqlite3_open_v2( pszName, &hDB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, pMyVFS->zName );
    }
    else
        rc = sqlite3_open_v2( pszName, &hDB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL );
#else
    rc = sqlite3_open( pszName, &hDB );
#endif
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "sqlite3_open(%s) failed: %s", 
                  pszName, sqlite3_errmsg( hDB ) );
#ifdef HAVE_SQLITE_VFS
        if (pMyVFS) sqlite3_vfs_unregister(pMyVFS);
        CPLFree(pMyVFS);
#endif
        return NULL;
    }

    int bMetadata = CSLFetchBoolean( papszOptions, "METADATA", TRUE );

    CPLString osCommand;
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
            sqlite3_close( hDB );
#ifdef HAVE_SQLITE_VFS
            if (pMyVFS) sqlite3_vfs_unregister(pMyVFS);
            CPLFree(pMyVFS);
#endif
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the SpatiaLite metadata tables.                          */
/* -------------------------------------------------------------------- */
    if ( bSpatialite )
    {
        /* 
        / SpatiaLite full support: calling InitSpatialMetadata() 
        /
        / IMPORTANT NOTICE: on SpatiaLite any attempt aimed
        / to directly CREATE "geometry_columns" and "spatial_ref_sys" 
        / [by-passing InitSpatialMetadata() as absolutely required]
        / will severely [and irremediably] corrupt the DB !!!
        */
        osCommand =  "SELECT InitSpatialMetadata()";
        rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                "Unable to Initialize SpatiaLite Metadata: %s",
                    pszErrMsg );
            sqlite3_free( pszErrMsg );
            sqlite3_close( hDB );
#ifdef HAVE_SQLITE_VFS
            if (pMyVFS) sqlite3_vfs_unregister(pMyVFS);
            CPLFree(pMyVFS);
#endif
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*  Create the geometry_columns and spatial_ref_sys metadata tables.    */
/* -------------------------------------------------------------------- */
    else if( bMetadata )
    {
        osCommand = 
            "CREATE TABLE geometry_columns ("
            "     f_table_name VARCHAR, "
            "     f_geometry_column VARCHAR, "
            "     geometry_type INTEGER, "
            "     coord_dimension INTEGER, "
            "     srid INTEGER,"
            "     geometry_format VARCHAR )";
        rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create table geometry_columns: %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            sqlite3_close( hDB );
#ifdef HAVE_SQLITE_VFS
            if (pMyVFS) sqlite3_vfs_unregister(pMyVFS);
            CPLFree(pMyVFS);
#endif
            return NULL;
        }

        osCommand = 
            "CREATE TABLE spatial_ref_sys        ("
            "     srid INTEGER UNIQUE,"
            "     auth_name TEXT,"
            "     auth_srid TEXT,"
            "     srtext TEXT)";
        rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create table spatial_ref_sys: %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            sqlite3_close( hDB );
#ifdef HAVE_SQLITE_VFS
            if (pMyVFS) sqlite3_vfs_unregister(pMyVFS);
            CPLFree(pMyVFS);
#endif
            return NULL;
        }
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Close the DB file so we can reopen it normally.                 */
/* -------------------------------------------------------------------- */
        sqlite3_close( hDB );
#ifdef HAVE_SQLITE_VFS
        if (pMyVFS) sqlite3_vfs_unregister(pMyVFS);
        CPLFree(pMyVFS);
#endif

        OGRSQLiteDataSource     *poDS;
        poDS = new OGRSQLiteDataSource();

        if( !poDS->Open( pszName, TRUE ) )
        {
            delete poDS;
            return NULL;
        }
        else
            return poDS;
    }

/* -------------------------------------------------------------------- */
/*      Optionnaly initialize the content of the spatial_ref_sys table  */
/*      with the EPSG database                                          */
/* -------------------------------------------------------------------- */
    if ( (bSpatialite || bMetadata) &&
         CSLFetchBoolean( papszOptions, "INIT_WITH_EPSG", FALSE ) )
    {
        InitWithEPSG(hDB, bSpatialite);
    }

/* -------------------------------------------------------------------- */
/*      Close the DB file so we can reopen it normally.                 */
/* -------------------------------------------------------------------- */
    sqlite3_close( hDB );

#ifdef HAVE_SQLITE_VFS
    if (pMyVFS) sqlite3_vfs_unregister(pMyVFS);
    CPLFree(pMyVFS);
#endif

    return Open( pszName, TRUE );
}

/************************************************************************/
/*                           InitWithEPSG()                             */
/************************************************************************/

int OGRSQLiteDriver::InitWithEPSG(sqlite3* hDB, int bSpatialite)
{
    CPLString osCommand;
    char* pszErrMsg = NULL;

    if ( bSpatialite )
    {
        /* 
        / if v.2.4.0 (or any subsequent) InitWithEPSG make no sense at all
        / because the EPSG dataset is already self-initialized at DB creation
        */
        int iSpatialiteVersion = OGRSQLiteGetSpatialiteVersionNumber();
        if ( iSpatialiteVersion >= 24 )
            return TRUE;
    }
	
    int rc = sqlite3_exec( hDB, "BEGIN", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                "Unable to insert into spatial_ref_sys: %s",
                pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    FILE* fp;
    int i;
    for(i=0;i<2 && rc == SQLITE_OK;i++)
    {
        const char* pszFilename = (i == 0) ? "gcs.csv" : "pcs.csv";
        fp = VSIFOpen(CSVFilename(pszFilename), "rt");
        if (fp == NULL)
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                "Unable to open EPSG support file %s.\n"
                "Try setting the GDAL_DATA environment variable to point to the\n"
                "directory containing EPSG csv files.", 
                pszFilename );

            continue;
        }

        OGRSpatialReference oSRS;
        char** papszTokens;
        CSLDestroy(CSVReadParseLine( fp ));
        while ( (papszTokens = CSVReadParseLine( fp )) != NULL && rc == SQLITE_OK)
        {
            int nSRSId = atoi(papszTokens[0]);
            CSLDestroy(papszTokens);

            CPLPushErrorHandler(CPLQuietErrorHandler);
            oSRS.importFromEPSG(nSRSId);
            CPLPopErrorHandler();

            if (bSpatialite)
            {
                char    *pszProj4 = NULL;

                CPLPushErrorHandler(CPLQuietErrorHandler);
                OGRErr eErr = oSRS.exportToProj4( &pszProj4 );
                CPLPopErrorHandler();

                char    *pszWKT = NULL;
                if( oSRS.exportToWkt( &pszWKT ) != OGRERR_NONE )
                {
                    CPLFree(pszWKT);
                    pszWKT = NULL;
                }

                if( eErr == OGRERR_NONE )
                {
                    const char  *pszProjCS = oSRS.GetAttrValue("PROJCS");
                    if (pszProjCS == NULL)
                        pszProjCS = oSRS.GetAttrValue("GEOGCS");

                    int bHasSrsWkt = FALSE;

                /* testing for SRS_WKT column presence */
                    char **papszResult;
                    int nRowCount, nColCount;
                    rc = sqlite3_get_table( hDB, "PRAGMA table_info(spatial_ref_sys)", 
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

                    if (bHasSrsWkt == TRUE)
                    {
                    /* the SPATIAL_REF_SYS table supports a SRS_WKT column */
                        if ( pszProjCS )
                            osCommand.Printf(
                                "INSERT INTO spatial_ref_sys "
                                "(srid, auth_name, auth_srid, ref_sys_name, proj4text, srs_wkt) "
                                "VALUES (%d, 'EPSG', '%d', ?, ?, ?)",
                                nSRSId, nSRSId);
                        else
                            osCommand.Printf(
                                "INSERT INTO spatial_ref_sys "
                                "(srid, auth_name, auth_srid, proj4text, srs_wkt) "
                                "VALUES (%d, 'EPSG', '%d', ?, ?)",
                                nSRSId, nSRSId);
                    }
                    else
                    {
                    /* the SPATIAL_REF_SYS table does not support a SRS_WKT column */
                        if ( pszProjCS )
                            osCommand.Printf(
                                "INSERT INTO spatial_ref_sys "
                                "(srid, auth_name, auth_srid, ref_sys_name, proj4text) "
                                "VALUES (%d, 'EPSG', '%d', ?, ?)",
                                nSRSId, nSRSId);
                        else
                            osCommand.Printf(
                                "INSERT INTO spatial_ref_sys "
                                "(srid, auth_name, auth_srid, proj4text) "
                                "VALUES (%d, 'EPSG', '%d', ?)",
                                nSRSId, nSRSId);
                    }

                    sqlite3_stmt *hInsertStmt = NULL;
                    rc = sqlite3_prepare( hDB, osCommand, -1, &hInsertStmt, NULL );

                    if ( pszProjCS )
                    {
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 1, pszProjCS, -1, SQLITE_STATIC );
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 2, pszProj4, -1, SQLITE_STATIC );
                        if (bHasSrsWkt == TRUE)
                        {
                        /* the SPATIAL_REF_SYS table supports a SRS_WKT column */
                            if( rc == SQLITE_OK && pszWKT != NULL)
                                rc = sqlite3_bind_text( hInsertStmt, 3, pszWKT, -1, SQLITE_STATIC );
                        }
                    }
                    else
                    {
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 1, pszProj4, -1, SQLITE_STATIC );
                        if (bHasSrsWkt == TRUE)
                        {
                        /* the SPATIAL_REF_SYS table supports a SRS_WKT column */
                            if( rc == SQLITE_OK && pszWKT != NULL)
                                rc = sqlite3_bind_text( hInsertStmt, 2, pszWKT, -1, SQLITE_STATIC );
                        }
                    }

                    if( rc == SQLITE_OK)
                        rc = sqlite3_step( hInsertStmt );

                    if( rc != SQLITE_OK && rc != SQLITE_DONE )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined, 
                                    "Cannot insert %s into spatial_ref_sys : %s",
                                    pszProj4,
                                    sqlite3_errmsg(hDB) );

                        sqlite3_finalize( hInsertStmt );
                        CPLFree(pszProj4);
                        CPLFree(pszWKT);
                        break;
                    }
                    rc = SQLITE_OK;

                    sqlite3_finalize( hInsertStmt );
                }

                CPLFree(pszProj4);
                CPLFree(pszWKT);
            }
            else
            {
                char    *pszWKT = NULL;
                if( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE )
                {
                    osCommand.Printf(
                        "INSERT INTO spatial_ref_sys "
                        "(srid, auth_name, auth_srid, srtext) "
                        "VALUES (%d, 'EPSG', '%d', ?)",
                        nSRSId, nSRSId );

                    sqlite3_stmt *hInsertStmt = NULL;
                    rc = sqlite3_prepare( hDB, osCommand, -1, &hInsertStmt, NULL );

                    if( rc == SQLITE_OK)
                        rc = sqlite3_bind_text( hInsertStmt, 1, pszWKT, -1, SQLITE_STATIC );

                    if( rc == SQLITE_OK)
                        rc = sqlite3_step( hInsertStmt );

                    if( rc != SQLITE_OK && rc != SQLITE_DONE )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined, 
                                    "Cannot insert %s into spatial_ref_sys : %s",
                                    pszWKT,
                                    sqlite3_errmsg(hDB) );

                        sqlite3_finalize( hInsertStmt );
                        CPLFree(pszWKT);
                        break;
                    }
                    rc = SQLITE_OK;

                    sqlite3_finalize( hInsertStmt );
                }

                CPLFree(pszWKT);
            }
        }
        VSIFClose(fp);
    }

    if (rc == SQLITE_OK)
        rc = sqlite3_exec( hDB, "COMMIT", NULL, NULL, &pszErrMsg );
    else
        rc = sqlite3_exec( hDB, "ROLLBACK", NULL, NULL, &pszErrMsg );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                "Unable to insert into spatial_ref_sys: %s",
                pszErrMsg );
        sqlite3_free( pszErrMsg );
    }

    return (rc == SQLITE_OK);
}

/************************************************************************/
/*                         DeleteDataSource()                           */
/************************************************************************/

OGRErr OGRSQLiteDriver::DeleteDataSource( const char *pszName )
{
    if (VSIUnlink( pszName ) == 0)
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                         RegisterOGRSQLite()                          */
/************************************************************************/

void RegisterOGRSQLite()

{
    if (! GDAL_CHECK_VERSION("SQLite driver"))
        return;
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRSQLiteDriver );
}

