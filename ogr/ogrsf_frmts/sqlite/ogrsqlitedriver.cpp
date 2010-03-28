/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
    FILE *fpDB;
    char szHeader[16];
    
    fpDB = VSIFOpen( pszFilename, "rb" );
    if( fpDB == NULL )
        return NULL;
    
    if( VSIFRead( szHeader, 1, 16, fpDB ) != 16 )
        memset( szHeader, 0, 16 );
    
    VSIFClose( fpDB );
    
    if( strncmp( szHeader, "SQLite format 3", 15 ) != 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      We think this is really an SQLite database, go ahead and try    */
/*      and open it.                                                    */
/* -------------------------------------------------------------------- */
    OGRSQLiteDataSource     *poDS;

    poDS = new OGRSQLiteDataSource();

    if( !poDS->Open( pszFilename ) )
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
    VSIStatBuf sStatBuf;

    if( VSIStat( pszName, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "It seems a file system object called '%s' already exists.",
                  pszName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the database file.                                       */
/* -------------------------------------------------------------------- */
    sqlite3             *hDB;
    int rc;

    hDB = NULL;
    rc = sqlite3_open( pszName, &hDB );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "sqlite3_open(%s) failed: %s", 
                  pszName, sqlite3_errmsg( hDB ) );
        return NULL;
    }

    int bSpatialite = CSLFetchBoolean( papszOptions, "SPATIALITE", FALSE );
    int bMetadata = CSLFetchBoolean( papszOptions, "METADATA", TRUE );

    CPLString osCommand;
    char *pszErrMsg = NULL;

/* -------------------------------------------------------------------- */
/*      Create the SpatiaLite metadata tables.                          */
/* -------------------------------------------------------------------- */
    if ( bSpatialite )
    {
        osCommand = 
            "CREATE TABLE geometry_columns ("
            "     f_table_name VARCHAR NOT NULL, "
            "     f_geometry_column VARCHAR NOT NULL, "
            "     type VARCHAR NOT NULL, "
            "     coord_dimension INTEGER NOT NULL, "
            "     srid INTEGER,"
            "     spatial_index_enabled INTEGER NOT NULL)";
        rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create table geometry_columns: %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            return NULL;
        }

        osCommand = 
            "CREATE TABLE spatial_ref_sys        ("
            "     srid INTEGER NOT NULL PRIMARY KEY,"
            "     auth_name VARCHAR NOT NULL,"
            "     auth_srid INTEGER NOT NULL,"
            "     ref_sys_name VARCHAR,"
            "     proj4text VARCHAR NOT NULL)";
        rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create table spatial_ref_sys: %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            return NULL;
        }

        osCommand = 
            "CREATE VIEW geom_cols_ref_sys AS"
            "   SELECT f_table_name, f_geometry_column, type,"
            "          coord_dimension, spatial_ref_sys.srid AS srid,"
            "          auth_name, auth_srid, ref_sys_name, proj4text"
            "   FROM geometry_columns, spatial_ref_sys"
            "   WHERE geometry_columns.srid = spatial_ref_sys.srid";
        rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create view geom_cols_ref_sys: %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
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
            return NULL;
        }
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Close the DB file so we can reopen it normally.                 */
/* -------------------------------------------------------------------- */
        sqlite3_close( hDB );

        OGRSQLiteDataSource     *poDS;
        poDS = new OGRSQLiteDataSource();

        if( !poDS->Open( pszName ) )
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

    return Open( pszName, TRUE );
}

/************************************************************************/
/*                           InitWithEPSG()                             */
/************************************************************************/

int OGRSQLiteDriver::InitWithEPSG(sqlite3* hDB, int bSpatialite)
{
    CPLString osCommand;
    char* pszErrMsg = NULL;

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

                if( eErr == OGRERR_NONE )
                {
                    const char  *pszProjCS = oSRS.GetAttrValue("PROJCS");
                    if (pszProjCS == NULL)
                        pszProjCS = oSRS.GetAttrValue("GEOGCS");

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

                    sqlite3_stmt *hInsertStmt = NULL;
                    rc = sqlite3_prepare( hDB, osCommand, -1, &hInsertStmt, NULL );

                    if ( pszProjCS )
                    {
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 1, pszProjCS, -1, SQLITE_STATIC );
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 2, pszProj4, -1, SQLITE_STATIC );
                    }
                    else
                    {
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 1, pszProj4, -1, SQLITE_STATIC );
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
                        break;
                    }
                    rc = SQLITE_OK;

                    sqlite3_finalize( hInsertStmt );
                }

                CPLFree(pszProj4);
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
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
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

