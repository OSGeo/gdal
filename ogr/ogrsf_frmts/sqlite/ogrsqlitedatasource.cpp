/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2004/07/09 06:25:04  warmerda
 * New
 *
 * Revision 1.5  2004/01/05 22:38:17  warmerda
 * stripped out some junk
 *
 * Revision 1.4  2004/01/05 22:23:40  warmerda
 * added ExecuteSQL implementation via OGRSQLiteSelectLayer
 *
 * Revision 1.3  2003/11/10 20:08:50  warmerda
 * support explicit tables list, or GetTables() fallback
 *
 * Revision 1.2  2003/10/06 19:18:30  warmerda
 * Added userid/password support
 *
 * Revision 1.1  2003/09/25 17:08:37  warmerda
 * New
 *
 */

#include "ogr_sqlite.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");
/************************************************************************/
/*                         OGRSQLiteDataSource()                          */
/************************************************************************/

OGRSQLiteDataSource::OGRSQLiteDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;

    nKnownSRID = 0;
    panSRID = NULL;
    papoSRS = NULL;
}

/************************************************************************/
/*                         ~OGRSQLiteDataSource()                         */
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
        if( papoSRS[i] != NULL && papoSRS[i]->Dereference() == 0 )
            delete papoSRS[i];
    }
    CPLFree( panSRID );
    CPLFree( papoSRS );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSQLiteDataSource::Open( const char * pszNewName, int bUpdate,
                              int bTestOpen )

{
    CPLAssert( nLayers == 0 );

    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Verify that the target is a real file, and has an               */
/*      appropriate magic string at the beginning.                      */
/* -------------------------------------------------------------------- */
    if( bTestOpen )
    {
        FILE *fpDB;
        char szHeader[16];

        fpDB = VSIFOpen( pszNewName, "rb" );
        if( fpDB == NULL )
            return FALSE;

        if( VSIFRead( szHeader, 1, 16, fpDB ) != 16 )
            memset( szHeader, 0, 16 );

        VSIFClose( fpDB );
        
        if( strncmp( szHeader, "SQLite format 3", 15 ) != 0 )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try to open the sqlite database properly now.                   */
/* -------------------------------------------------------------------- */
    int rc;

    hDB = NULL;
    rc = sqlite3_open( pszNewName, &hDB );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "sqlite3_open(%s) failed: %d", 
                  pszNewName, sqlite3_errmsg( hDB ) );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If we have a GEOMETRY_COLUMN tables, initialize on the basis    */
/*      of that.                                                        */
/* -------------------------------------------------------------------- */
    char **papszResult;
    int nRowCount, iRow, nColCount;
    char *pszErrMsg;

    rc = sqlite3_get_table( 
        hDB,
        "SELECT f_table_name, f_geometry_column, geometry_type"
        " FROM geometry_columns",
        &papszResult, &nRowCount, &nColCount, &pszErrMsg );

    if( rc == SQLITE_OK )
    {
        for( iRow = 0; iRow < nRowCount; iRow++ )
        {
            char **papszRow = papszResult + iRow * 3 + 3;

            OpenTable( papszRow[0], papszRow[1], bUpdate );
        }

        sqlite3_free_table(papszResult);
    }
    else
        sqlite3_free( pszErrMsg );

/* -------------------------------------------------------------------- */
/*      Otherwise our final resort is to return all tables and views    */
/*      as non-spatial tables.                                          */
/* -------------------------------------------------------------------- */
    rc = sqlite3_get_table( hDB,
                           "SELECT name FROM sqlite_master "
                           "WHERE type IN ('table','view') "
                           "UNION ALL "
                           "SELECT name FROM sqlite_temp_master "
                           "WHERE type IN ('table','view') "
                           "ORDER BY 1",
                            &papszResult, &nRowCount, &nColCount, &pszErrMsg );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to fetch list of tables: %s", 
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    for( iRow = 0; iRow < nRowCount; iRow++ )
    {
        OpenTable( papszResult[iRow+1], NULL, bUpdate );
    }
    
    sqlite3_free_table(papszResult);

    return TRUE;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRSQLiteDataSource::OpenTable( const char *pszNewName, 
                                  const char *pszGeomCol,
                                  int bUpdate )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteTableLayer  *poLayer;

    poLayer = new OGRSQLiteTableLayer( this );

    if( poLayer->Initialize( pszNewName, pszGeomCol ) )
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
        return TRUE;
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

OGRLayer * OGRSQLiteDataSource::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
    return NULL;
#ifdef notdef
    CPLSQLiteStatement *poStmt = new CPLSQLiteStatement( &oSession );

    poStmt->Append( pszSQLCommand );
    if( !poStmt->ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", oSession.GetLastError() );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Are there result columns for this statement?                    */
/* -------------------------------------------------------------------- */
    if( poStmt->GetColCount() == 0 )
    {
        delete poStmt;
        CPLErrorReset();
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a results layer.  It will take ownership of the          */
/*      statement.                                                      */
/* -------------------------------------------------------------------- */
    OGRSQLiteSelectLayer *poLayer = NULL;
        
    poLayer = new OGRSQLiteSelectLayer( this, poStmt );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );
    
    return poLayer;
#endif
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRSQLiteDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}
