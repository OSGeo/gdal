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

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ~OGRSQLiteDriver()                            */
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

/* -------------------------------------------------------------------- */
/*      Create the SpatiaLite metadata tables.                          */
/* -------------------------------------------------------------------- */
    if ( CSLFetchBoolean( papszOptions, "SPATIALITE", FALSE ) )
    {
        CPLString osCommand;
        char *pszErrMsg = NULL;

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
    }

/* -------------------------------------------------------------------- */
/*  Create the geometry_columns and spatial_ref_sys metadata tables.    */
/* -------------------------------------------------------------------- */
    else if( CSLFetchBoolean( papszOptions, "METADATA", TRUE ) )
    {
        CPLString osCommand;
        char *pszErrMsg = NULL;

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
/*      Close the DB file so we can reopen it normally.                 */
/* -------------------------------------------------------------------- */
    sqlite3_close( hDB );

    return Open( pszName, TRUE );
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

