/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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
 * Revision 1.9  2005/05/16 19:56:05  fwarmerdam
 * in new versions _rowid_ is ogc_fid, handle gracefully
 *
 * Revision 1.8  2005/02/22 12:50:31  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.7  2004/07/13 15:47:34  warmerda
 * fixed similar quoting bug in GetFeature()
 *
 * Revision 1.6  2004/07/13 15:38:44  warmerda
 * Fixed quoting in DELETE statement.  Don't use single quotes on field
 * names in the WHERE clause.
 *
 * Revision 1.5  2004/07/13 15:11:19  warmerda
 * implemented SetFeature, transaction support
 *
 * Revision 1.4  2004/07/12 21:50:59  warmerda
 * fixed up SQL escaping
 *
 * Revision 1.3  2004/07/12 20:50:46  warmerda
 * table/database creation now implemented
 *
 * Revision 1.2  2004/07/11 19:23:51  warmerda
 * read implementation working well
 *
 * Revision 1.1  2004/07/09 06:25:05  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_sqlite.h"
#include <string>

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRSQLiteTableLayer()                         */
/************************************************************************/

OGRSQLiteTableLayer::OGRSQLiteTableLayer( OGRSQLiteDataSource *poDSIn )

{
    poDS = poDSIn;

    pszQuery = NULL;

    bUpdateAccess = TRUE;

    iNextShapeId = 0;

    nSRSId = -1;

    poFeatureDefn = NULL;
}

/************************************************************************/
/*                        ~OGRSQLiteTableLayer()                        */
/************************************************************************/

OGRSQLiteTableLayer::~OGRSQLiteTableLayer()

{
    CPLFree( pszQuery );
    ClearStatement();
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRSQLiteTableLayer::Initialize( const char *pszTableName, 
                                        const char *pszGeomCol )

{
    sqlite3 *hDB = poDS->GetDB();

    CPLFree( pszGeomColumn );
    if( pszGeomCol == NULL )
        pszGeomColumn = NULL;
    else
        pszGeomColumn = CPLStrdup( pszGeomCol );

    CPLFree( pszFIDColumn );
    pszFIDColumn = NULL;

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    CPLErr eErr;
    sqlite3_stmt *hColStmt = NULL;
    const char *pszSQL = CPLSPrintf( "SELECT _rowid_, * FROM '%s' LIMIT 1",
                                     pszTableName );

    if( sqlite3_prepare( hDB, pszSQL, strlen(pszSQL), &hColStmt, NULL )
        != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to query table %s for column definitions.",
                  pszTableName );
        
        return CE_Failure;
    }
    
    sqlite3_step( hColStmt );

/* -------------------------------------------------------------------- */
/*      What should we use as FID?  If there is a primary key           */
/*      integer field, then this will be used as the _rowid_, and we    */
/*      will pick up the real column name here.  Otherwise, we will     */
/*      just use fid.                                                   */
/*                                                                      */
/*      Note that the select _rowid_ will return the real column        */
/*      name if the rowid corresponds to another primary key            */
/*      column.                                                         */
/* -------------------------------------------------------------------- */
    pszFIDColumn = CPLStrdup(sqlite3_column_name( hColStmt, 0 ));

/* -------------------------------------------------------------------- */
/*      Collect the rest of the fields.                                 */
/* -------------------------------------------------------------------- */
    eErr = BuildFeatureDefn( pszTableName, hColStmt );
    if( eErr != CE_None )
        return eErr;

#ifdef notdef
    if( poFeatureDefn->GetFieldCount() == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "No column definitions found for table '%s', layer not usable.", 
                  pszTableName );
        return CE_Failure;
    }
#endif

    sqlite3_finalize( hColStmt );

    return CE_None;
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRSQLiteTableLayer::ClearStatement()

{
    if( hStmt != NULL )
    {
        CPLDebug( "OGR_SQLITE", "finalize %p", hStmt );
	sqlite3_finalize( hStmt );
        hStmt = NULL;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

sqlite3_stmt *OGRSQLiteTableLayer::GetStatement()

{
    if( hStmt == NULL )
        ResetStatement();

    return hStmt;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::ResetStatement()

{
    int rc;
    char *pszSQL;

    ClearStatement();

    iNextShapeId = 0;

    if( pszQuery != NULL )
        pszSQL = CPLStrdup( 
            CPLSPrintf( "SELECT _rowid_, * FROM '%s' WHERE %s", 
                        poFeatureDefn->GetName(), 
                        pszQuery ) );
    else
        pszSQL = CPLStrdup( CPLSPrintf( "SELECT _rowid_, * FROM '%s'", 
				        poFeatureDefn->GetName() ) );

    rc = sqlite3_prepare( poDS->GetDB(), pszSQL, strlen(pszSQL), 
	    	          &hStmt, NULL );

    CPLDebug( "OGR_SQLITE", "prepare(%s) -> %p", pszSQL, hStmt );
    if( rc == SQLITE_OK )
    {
        CPLFree( pszSQL );
    	return OGRERR_NONE;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "sqlite3_prepare(%s):\n  %s", 
                  pszSQL, sqlite3_errmsg(poDS->GetDB()) );
        hStmt = NULL;
        CPLFree( pszSQL );
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSQLiteTableLayer::ResetReading()

{
    ClearStatement();
    OGRSQLiteLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRSQLiteTableLayer::GetFeature( long nFeatureId )

{
/* -------------------------------------------------------------------- */
/*      If we don't have an explicit FID column, just read through      */
/*      the result set iteratively to find our target.                  */
/* -------------------------------------------------------------------- */
    if( pszFIDColumn == NULL )
        return OGRSQLiteLayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Setup explicit query statement to fetch the record we want.     */
/* -------------------------------------------------------------------- */
    const char *pszSQL;
    int rc;

    ClearStatement();

    iNextShapeId = nFeatureId;

    pszSQL =
        CPLSPrintf( "SELECT _rowid_, * FROM '%s' WHERE \"%s\" = %d", 
                    poFeatureDefn->GetName(), 
                    pszFIDColumn, nFeatureId );

    CPLDebug( "OGR_SQLITE", "exec(%s)", pszSQL );

    rc = sqlite3_prepare( poDS->GetDB(), pszSQL, strlen(pszSQL), 
	    	          &hStmt, NULL );

/* -------------------------------------------------------------------- */
/*      Get the feature if possible.                                    */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = NULL;

    if( rc == SQLITE_OK )
        poFeature = GetNextRawFeature();

    ResetReading();

    return poFeature;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::SetAttributeFilter( const char *pszQuery )

{
    if( (pszQuery == NULL && this->pszQuery == NULL)
        || (pszQuery != NULL && this->pszQuery != NULL 
            && EQUAL(pszQuery,this->pszQuery)) )
        return OGRERR_NONE;

    CPLFree( this->pszQuery );
    if( pszQuery == NULL || strlen(pszQuery) == 0 )
        this->pszQuery = NULL;
    else
        this->pszQuery = CPLStrdup( pszQuery );

    ClearStatement();

    return OGRERR_NONE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    else 
        return OGRSQLiteLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGRSQLiteTableLayer::GetFeatureCount( int bForce )

{
    if( m_poFilterGeom != NULL && pszGeomColumn != NULL )
        return OGRSQLiteLayer::GetFeatureCount( bForce );

/* -------------------------------------------------------------------- */
/*      Form count SQL.                                                 */
/* -------------------------------------------------------------------- */
    const char *pszSQL;

    if( pszQuery != NULL )
        pszSQL = CPLSPrintf( "SELECT count(*) FROM '%s' WHERE %s",
                             poFeatureDefn->GetName(), pszQuery );
    else
        pszSQL = CPLSPrintf( "SELECT count(*) FROM '%s'",
                             poFeatureDefn->GetName() );

/* -------------------------------------------------------------------- */
/*      Execute.                                                        */
/* -------------------------------------------------------------------- */
    char **papszResult, *pszErrMsg;
    int nRowCount, nColCount;
    int nResult = -1;

    if( sqlite3_get_table( poDS->GetDB(), pszSQL, &papszResult, 
                           &nColCount, &nRowCount, &pszErrMsg ) != SQLITE_OK )
        return -1;

    if( nRowCount == 1 && nColCount == 1 )
        nResult = atoi(papszResult[1]);

    sqlite3_free_table( papszResult );

    return nResult;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/*                                                                      */
/*      We override this to try and fetch the table SRID from the       */
/*      geometry_columns table if the srsid is -2 (meaning we           */
/*      haven't yet even looked for it).                                */
/************************************************************************/

OGRSpatialReference *OGRSQLiteTableLayer::GetSpatialRef()

{
#ifdef notdef
    if( nSRSId == -2 )
    {
        PGconn          *hPGConn = poDS->GetPGConn();
        PGresult        *hResult;
        char            szCommand[1024];

        nSRSId = -1;

        poDS->SoftStartTransaction();

        sprintf( szCommand, 
                 "SELECT srid FROM geometry_columns "
                 "WHERE f_table_name = '%s'",
                 poFeatureDefn->GetName() );
        hResult = PQexec(hPGConn, szCommand );

        if( hResult 
            && PQresultStatus(hResult) == PGRES_TUPLES_OK 
            && PQntuples(hResult) == 1 )
        {
            nSRSId = atoi(PQgetvalue(hResult,0,0));
        }

        poDS->SoftCommit();
    }
#endif

    return OGRSQLiteLayer::GetSpatialRef();
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::CreateField( OGRFieldDefn *poFieldIn, 
                                         int bApproxOK )

{
    char                szFieldType[256];
    OGRFieldDefn        oField( poFieldIn );

    ResetReading();

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = poDS->LaunderName( oField.GetNameRef() );

        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );
    }
    
/* -------------------------------------------------------------------- */
/*      Work out the PostgreSQL type.                                   */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
    {
        strcpy( szFieldType, "INTEGER" );
    }
    else if( oField.GetType() == OFTReal )
    {
        strcpy( szFieldType, "FLOAT" );
    }
    else
    {
        strcpy( szFieldType, "VARCHAR" );
    }

/* -------------------------------------------------------------------- */
/*      How much space do we need for the list of fields.               */
/* -------------------------------------------------------------------- */
    int iField, nFieldListLen = 100;
    char *pszOldFieldList, *pszNewFieldList;

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        nFieldListLen += 
            strlen(poFeatureDefn->GetFieldDefn(iField)->GetNameRef()) + 50;
    }

    nFieldListLen += strlen( oField.GetNameRef() );

    pszOldFieldList = (char *) CPLCalloc(1,nFieldListLen);
    pszNewFieldList = (char *) CPLCalloc(1,nFieldListLen);

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    const char *pszType;
    sprintf( pszOldFieldList, "%s", "OGC_FID" );
    sprintf( pszNewFieldList, "%s", "OGC_FID INTEGER PRIMARY KEY" );
    
    int iNextOrdinal = 3; /* _rowid_ is 1, OGC_FID is 2 */

    if( poFeatureDefn->GetGeomType() != wkbNone )
    {
        strcat( pszOldFieldList, ", WKT_GEOMETRY" );
        strcat( pszNewFieldList, ", WKT_GEOMETRY VARCHAR" );
        iNextOrdinal++;
    }

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(iField);

        // we already added OGC_FID so don't do it again
        if( EQUAL(poFldDefn->GetNameRef(),"OGC_FID") )
            continue;

        if( poFldDefn->GetType() == OFTInteger )
            pszType = "INTEGER";
        else if( poFldDefn->GetType() == OFTReal )
            pszType = "FLOAT";
        else
            pszType = "VARCHAR";
        
        sprintf( pszOldFieldList+strlen(pszOldFieldList), 
                 ", '%s'", poFldDefn->GetNameRef() );

        sprintf( pszNewFieldList+strlen(pszNewFieldList), 
                 ", '%s' %s", poFldDefn->GetNameRef(), pszType );

        iNextOrdinal++;
    }

/* -------------------------------------------------------------------- */
/*      Add the new field.                                              */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
        pszType = "INTEGER";
    else if( oField.GetType() == OFTReal )
        pszType = "FLOAT";
    else
        pszType = "VARCHAR";
    
    sprintf( pszNewFieldList+strlen(pszNewFieldList), 
             ", '%s' %s", oField.GetNameRef(), pszType );

/* ==================================================================== */
/*      Backup, destroy, recreate and repopulate the table.  SQLite     */
/*      has no ALTER TABLE so we have to do all this to add a           */
/*      column.                                                         */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Do this all in a transaction.                                   */
/* -------------------------------------------------------------------- */
    poDS->SoftStartTransaction();

/* -------------------------------------------------------------------- */
/*      Make a backup of the table.                                     */
/* -------------------------------------------------------------------- */
    int rc;
    char *pszErrMsg = NULL;
    sqlite3 *hDB = poDS->GetDB();
    
    rc = sqlite3_exec( hDB, 
                       CPLSPrintf( "CREATE TEMPORARY TABLE t1_back(%s)",
                                   pszOldFieldList ),
                       NULL, NULL, &pszErrMsg );

    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB, 
                           CPLSPrintf( "INSERT INTO t1_back SELECT %s FROM '%s'",
                                       pszOldFieldList, 
                                       poFeatureDefn->GetName() ),
                           NULL, NULL, &pszErrMsg );


/* -------------------------------------------------------------------- */
/*      Drop the original table, and recreate with new field.           */
/* -------------------------------------------------------------------- */
    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB, 
                           CPLSPrintf( "DROP TABLE '%s'", 
                                       poFeatureDefn->GetName() ),
                           NULL, NULL, &pszErrMsg );

    if( rc == SQLITE_OK )
    {
        const char *pszCmd = 
            CPLSPrintf( "CREATE TABLE '%s' (%s)", 
                        poFeatureDefn->GetName(),
                        pszNewFieldList );
        rc = sqlite3_exec( hDB, pszCmd, 
                           NULL, NULL, &pszErrMsg );

        CPLDebug( "OGR_SQLITE", "exec(%s)", pszCmd );
    }

/* -------------------------------------------------------------------- */
/*      Copy backup field values into new table.                        */
/* -------------------------------------------------------------------- */
    
    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB, 
                           CPLSPrintf( "INSERT INTO '%s' SELECT %s, NULL FROM t1_back",
                                       poFeatureDefn->GetName(),
                                       pszOldFieldList ),
                           NULL, NULL, &pszErrMsg );


/* -------------------------------------------------------------------- */
/*      Cleanup backup table.                                           */
/* -------------------------------------------------------------------- */
    
    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB, 
                           CPLSPrintf( "DROP TABLE t1_back" ),
                           NULL, NULL, &pszErrMsg );

/* -------------------------------------------------------------------- */
/*      COMMIT on success or ROLLBACK on failuire.                      */
/* -------------------------------------------------------------------- */
    if( rc == SQLITE_OK )
    {
        poDS->SoftCommit();
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to add field %s to table %s:\n %s",
                  oField.GetNameRef(), poFeatureDefn->GetName(), 
                  pszErrMsg );
        sqlite3_free( pszErrMsg );

        poDS->SoftRollback();

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Add the field to the OGRFeatureDefn.                            */
/* -------------------------------------------------------------------- */
    int iNewField;

    poFeatureDefn->AddFieldDefn( &oField );

    iNewField = poFeatureDefn->GetFieldCount() - 1;
    panFieldOrdinals = (int *) 
        CPLRealloc(panFieldOrdinals, (iNewField+1) * sizeof(int) );
    panFieldOrdinals[iNewField] = iNextOrdinal;

    return OGRERR_NONE;
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::SetFeature( OGRFeature *poFeature )

{
    CPLAssert( pszFIDColumn != NULL );
    
    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SetFeature() with unset FID fails." );
        return OGRERR_FAILURE;
    }
/* -------------------------------------------------------------------- */
/*      Drop the record with this FID.                                  */
/* -------------------------------------------------------------------- */
    int rc;
    char *pszErrMsg = NULL;
    const char *pszSQL;

    pszSQL = 
        CPLSPrintf( "DELETE FROM '%s' WHERE \"%s\" = %d", 
                    poFeatureDefn->GetName(), 
                    pszFIDColumn,
                    poFeature->GetFID() );

    CPLDebug( "OGR_SQLITE", "exec(%s)", pszSQL );
    
    rc = sqlite3_exec( poDS->GetDB(), pszSQL,
                       NULL, NULL, &pszErrMsg );
    
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to delete old feature with FID %d failed.\n%s", 
                  poFeature->GetFID(), pszErrMsg );
        return OGRERR_FAILURE;
    }
    
/* -------------------------------------------------------------------- */
/*      Recreate the feature.                                           */
/* -------------------------------------------------------------------- */
    return CreateFeature( poFeature );
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::CreateFeature( OGRFeature *poFeature )

{
    std::string    oCommand;
    std::string    oValues;
    int            bNeedComma = FALSE;

    ResetReading();

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    oCommand += CPLSPrintf( "INSERT INTO '%s' (", poFeatureDefn->GetName() );

/* -------------------------------------------------------------------- */
/*      Add FID if we have a cleartext FID column.                      */
/* -------------------------------------------------------------------- */
    if( pszFIDColumn != NULL // && !EQUAL(pszFIDColumn,"OGC_FID") 
        && poFeature->GetFID() != OGRNullFID )
    {
        oCommand += pszFIDColumn;

        oValues += CPLSPrintf( "%d", poFeature->GetFID() );
        bNeedComma = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Add geometry.                                                   */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != NULL && poFeature->GetGeometryRef() != NULL )
    {
        char *pszWKT = NULL;

        if( bNeedComma )
        {
            oCommand += ",";
            oValues += ",";
        }

        oCommand += pszGeomColumn;

        poFeature->GetGeometryRef()->exportToWkt( &pszWKT );
        oValues += "'";
        oValues += pszWKT;
        oValues += "'";
        CPLFree( pszWKT );

        bNeedComma = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Add field values.                                               */
/* -------------------------------------------------------------------- */
    int iField;

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        const char *pszRawValue;

        if( !poFeature->IsFieldSet( iField ) )
            continue;

        if( bNeedComma )
        {
            oCommand += ",";
            oValues += ",";
        }

        oCommand += "'";
        oCommand +=poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
        oCommand += "'";

        pszRawValue = poFeature->GetFieldAsString( iField );
        if( strchr( pszRawValue, '\'' ) != NULL )
        {
            char *pszEscapedValue = 
                CPLEscapeString( pszRawValue, -1, CPLES_SQL );
            oValues += "'";
            oValues += pszEscapedValue;
            oValues += "'";

            CPLFree( pszEscapedValue );
        }
        else
        {
            oValues += "'";
            oValues += pszRawValue;
            oValues += "'";
        }
            
        bNeedComma = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Merge final command.                                            */
/* -------------------------------------------------------------------- */
    oCommand += ") VALUES (";
    oCommand += oValues;
    oCommand += ")";

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    int rc;
    char *pszErrMsg = NULL;

    CPLDebug( "OGR_SQLITE", "exec(%s)", oCommand.c_str() );

    rc = sqlite3_exec( poDS->GetDB(), oCommand.c_str(), 
                       NULL, NULL, &pszErrMsg );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "CreateFeature() failed: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      We likely ought to consider fetching the rowid/fid and          */
/*      putting it back in the feature.                                 */
/* -------------------------------------------------------------------- */
    // todo 

    return OGRERR_NONE;
}

