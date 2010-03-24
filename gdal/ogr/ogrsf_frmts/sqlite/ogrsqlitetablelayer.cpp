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
 ****************************************************************************/

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
    ClearStatement();
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRSQLiteTableLayer::Initialize( const char *pszTableName, 
                                        const char *pszGeomCol,
                                        OGRwkbGeometryType eGeomType,
                                        const char *pszGeomFormat,
                                        OGRSpatialReference *poSRS,
                                        int nSRSId,
                                        int bHasSpatialIndex)

{
    int rc;
    sqlite3 *hDB = poDS->GetDB();

    if( pszGeomCol == NULL )
        osGeomColumn = "";
    else
        osGeomColumn = pszGeomCol;

    if( pszGeomFormat )
    {
        if ( EQUAL(pszGeomFormat, "WKT") )
            eGeomFormat = OSGF_WKT;
        else if ( EQUAL(pszGeomFormat,"WKB") )
            eGeomFormat = OSGF_WKB;
        else if ( EQUAL(pszGeomFormat,"FGF") )
            eGeomFormat = OSGF_FGF;
        else if( EQUAL(pszGeomFormat,"SpatiaLite") )
            eGeomFormat = OSGF_SpatiaLite;
    }

    CPLFree( pszFIDColumn );
    pszFIDColumn = NULL;

    this->poSRS = poSRS;
    this->nSRSId = nSRSId;
    this->bHasSpatialIndex = bHasSpatialIndex;

    if( poSRS )
        poSRS->Reference();

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    CPLErr eErr;
    sqlite3_stmt *hColStmt = NULL;
    const char *pszSQL = CPLSPrintf( "SELECT _rowid_, * FROM '%s' LIMIT 1",
                                     pszTableName );

    rc = sqlite3_prepare( hDB, pszSQL, strlen(pszSQL), &hColStmt, NULL ); 
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to query table %s for column definitions : %s.",
                  pszTableName, sqlite3_errmsg(hDB) );
        
        return CE_Failure;
    }

    rc = sqlite3_step( hColStmt );
    if ( rc != SQLITE_DONE && rc != SQLITE_ROW )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "In Initialize(): sqlite3_step(%s):\n  %s", 
                  pszSQL, sqlite3_errmsg(hDB) );
        sqlite3_finalize( hColStmt );
        return CE_Failure;
    }

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
    sqlite3_finalize( hColStmt );

    if( eErr != CE_None )
        return eErr;


/* -------------------------------------------------------------------- */
/*      Set the geometry type if we know it.                            */
/* -------------------------------------------------------------------- */
    if( eGeomType != wkbUnknown )
        poFeatureDefn->SetGeomType( eGeomType );

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
    CPLString osSQL;

    ClearStatement();

    iNextShapeId = 0;

    osSQL.Printf( "SELECT _rowid_, * FROM '%s' %s", 
                    poFeatureDefn->GetName(), 
                    osWHERE.c_str() );

    rc = sqlite3_prepare( poDS->GetDB(), osSQL, osSQL.size(),
		          &hStmt, NULL );

    if( rc == SQLITE_OK )
    {
	return OGRERR_NONE;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "In ResetStatement(): sqlite3_prepare(%s):\n  %s", 
                  osSQL.c_str(), sqlite3_errmsg(poDS->GetDB()) );
        hStmt = NULL;
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
    CPLString osSQL;
    int rc;

    ClearStatement();

    iNextShapeId = nFeatureId;

    osSQL.Printf( "SELECT _rowid_, * FROM '%s' WHERE \"%s\" = %d", 
                  poFeatureDefn->GetName(), 
                  pszFIDColumn, (int) nFeatureId );

    CPLDebug( "OGR_SQLITE", "exec(%s)", osSQL.c_str() );

    rc = sqlite3_prepare( poDS->GetDB(), osSQL, osSQL.size(), 
                          &hStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "In GetFeature(): sqlite3_prepare(%s):\n  %s", 
                  osSQL.c_str(), sqlite3_errmsg(poDS->GetDB()) );

        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Get the feature if possible.                                    */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = NULL;

    poFeature = GetNextRawFeature();

    ResetReading();

    return poFeature;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::SetAttributeFilter( const char *pszQuery )

{
    if( pszQuery == NULL )
        osQuery = "";
    else
        osQuery = pszQuery;

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}


/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRSQLiteTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRSQLiteTableLayer::BuildWhere()

{
    osWHERE = "";

    if( m_poFilterGeom != NULL && bHasSpatialIndex )
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );
        osWHERE.Printf("WHERE MBRWithin(\"%s\", BuildMBR(%.12f, %.12f, %.12f, %.12f, %d)) ",
                       osGeomColumn.c_str(),
                       sEnvelope.MinX, sEnvelope.MinY,
                       sEnvelope.MaxX, sEnvelope.MaxY,
                       nSRSId);
    }

    if( strlen(osQuery) > 0 )
    {
        if( strlen(osWHERE) == 0 )
        {
            osWHERE.Printf( "WHERE %s ", osQuery.c_str()  );
        }
        else	
        {
            osWHERE += "AND ";
            osWHERE += osQuery;
        }
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteTableLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap,OLCFastFeatureCount))
        return m_poFilterGeom == NULL || osGeomColumn.size() == 0 ||
               bHasSpatialIndex;

    else if (EQUAL(pszCap,OLCFastSpatialFilter))
        return bHasSpatialIndex;

    else if( EQUAL(pszCap,OLCRandomRead) )
        return pszFIDColumn != NULL;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
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
    if( !TestCapability(OLCFastFeatureCount) )
        return OGRSQLiteLayer::GetFeatureCount( bForce );

/* -------------------------------------------------------------------- */
/*      Form count SQL.                                                 */
/* -------------------------------------------------------------------- */
    const char *pszSQL;

    pszSQL = CPLSPrintf( "SELECT count(*) FROM '%s' %s",
                            poFeatureDefn->GetName(), osWHERE.c_str() );

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
    return OGRSQLiteLayer::GetSpatialRef();
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::CreateField( OGRFieldDefn *poFieldIn, 
                                         int bApproxOK )

{
    OGRFieldDefn        oField( poFieldIn );

    ResetReading();

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into SQLite            */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = poDS->LaunderName( oField.GetNameRef() );

        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );
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
        strcat( pszOldFieldList, "," );
        strcat( pszNewFieldList, "," );

        strcat( pszOldFieldList, osGeomColumn );
        strcat( pszNewFieldList, osGeomColumn );

        if ( eGeomFormat == OSGF_WKT )
            strcat( pszNewFieldList, " VARCHAR" );
        else
            strcat( pszNewFieldList, " BLOB" );

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
        else if( poFldDefn->GetType() == OFTBinary )
            pszType = "BLOB";
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
    else if( oField.GetType() == OFTBinary )
        pszType = "BLOB";
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
/*      Save existing related triggers and index                        */
/* -------------------------------------------------------------------- */
    int rc;
    char *pszErrMsg = NULL;
    sqlite3 *hDB = poDS->GetDB();
    CPLString osSQL;

    osSQL.Printf( "SELECT sql FROM sqlite_master WHERE type IN ('trigger','index') AND tbl_name='%s'", 
                   poFeatureDefn->GetName() );

    int nRowTriggerIndexCount, nColTriggerIndexCount;
    char **papszTriggerIndexResult = NULL;
    rc = sqlite3_get_table( hDB, osSQL.c_str(), &papszTriggerIndexResult, 
                            &nRowTriggerIndexCount, &nColTriggerIndexCount, &pszErrMsg );

/* -------------------------------------------------------------------- */
/*      Make a backup of the table.                                     */
/* -------------------------------------------------------------------- */

    if( rc == SQLITE_OK )
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

    CPLFree( pszOldFieldList );
    CPLFree( pszNewFieldList );

/* -------------------------------------------------------------------- */
/*      Cleanup backup table.                                           */
/* -------------------------------------------------------------------- */
    
    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB, 
                           CPLSPrintf( "DROP TABLE t1_back" ),
                           NULL, NULL, &pszErrMsg );

/* -------------------------------------------------------------------- */
/*      Recreate existing related tables, triggers and index            */
/* -------------------------------------------------------------------- */

    if( rc == SQLITE_OK )
    {
        int i;

        for(i = 1; i <= nRowTriggerIndexCount && nColTriggerIndexCount == 1 && rc == SQLITE_OK; i++)
        {
            if (papszTriggerIndexResult[i] != NULL && papszTriggerIndexResult[i][0] != '\0')
                rc = sqlite3_exec( hDB, 
                            papszTriggerIndexResult[i],
                            NULL, NULL, &pszErrMsg );
        }
    }

/* -------------------------------------------------------------------- */
/*      COMMIT on success or ROLLBACK on failuire.                      */
/* -------------------------------------------------------------------- */

    sqlite3_free_table( papszTriggerIndexResult );

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
    if( pszFIDColumn == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SetFeature() without any FID column." );
        return OGRERR_FAILURE;
    }
    
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
        CPLSPrintf( "DELETE FROM '%s' WHERE \"%s\" = %ld", 
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
                  (int) poFeature->GetFID(), pszErrMsg );
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
    sqlite3 *hDB = poDS->GetDB();
    CPLString      osCommand;
    CPLString      osValues;
    int            bNeedComma = FALSE;

    ResetReading();

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    osCommand += CPLSPrintf( "INSERT INTO '%s' (", poFeatureDefn->GetName() );

/* -------------------------------------------------------------------- */
/*      Add FID if we have a cleartext FID column.                      */
/* -------------------------------------------------------------------- */
    if( pszFIDColumn != NULL // && !EQUAL(pszFIDColumn,"OGC_FID") 
        && poFeature->GetFID() != OGRNullFID )
    {
        osCommand += pszFIDColumn;

        osValues += CPLSPrintf( "%ld", poFeature->GetFID() );
        bNeedComma = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Add geometry.                                                   */
/* -------------------------------------------------------------------- */
    OGRGeometry *poGeom = poFeature->GetGeometryRef();

    if( osGeomColumn.size() != 0 &&
        poGeom != NULL &&
        eGeomFormat != OSGF_FGF )
    {

        if( bNeedComma )
        {
            osCommand += ",";
            osValues += ",";
        }

        osCommand += osGeomColumn;

        osValues += "?";

        bNeedComma = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Add field values.                                               */
/* -------------------------------------------------------------------- */
    int iField;
    int nFieldCount = poFeatureDefn->GetFieldCount();

    for( iField = 0; iField < nFieldCount; iField++ )
    {
        if( !poFeature->IsFieldSet( iField ) )
            continue;

        if( bNeedComma )
        {
            osCommand += ",";
            osValues += ",";
        }

        osCommand += "'";
        osCommand +=poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
        osCommand += "'";

        osValues += "?";

        bNeedComma = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Merge final command.                                            */
/* -------------------------------------------------------------------- */
    osCommand += ") VALUES (";
    osCommand += osValues;
    osCommand += ")";

/* -------------------------------------------------------------------- */
/*      Prepare the statement.                                          */
/* -------------------------------------------------------------------- */
    int rc;
    sqlite3_stmt *hInsertStmt;

#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "prepare(%s)", osCommand.c_str() );
#endif

    rc = sqlite3_prepare( hDB, osCommand, -1, &hInsertStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "In CreateFeature(): sqlite3_prepare(%s):\n  %s", 
                  osCommand.c_str(), sqlite3_errmsg(hDB) );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Bind the geometry                                               */
/* -------------------------------------------------------------------- */
    int nBindField = 1;

    if( osGeomColumn.size() != 0 &&
        poGeom != NULL &&
        eGeomFormat != OSGF_FGF )
    {
        if ( eGeomFormat == OSGF_WKT )
        {
            char *pszWKT = NULL;
            poGeom->exportToWkt( &pszWKT );
            rc = sqlite3_bind_text( hInsertStmt, nBindField++, pszWKT, -1, CPLFree );
        }
        else if( eGeomFormat == OSGF_WKB )
        {
            int nWKBLen = poGeom->WkbSize();
            GByte *pabyWKB = (GByte *) CPLMalloc(nWKBLen + 1);

            poGeom->exportToWkb( wkbNDR, pabyWKB );
            rc = sqlite3_bind_blob( hInsertStmt, nBindField++, pabyWKB, nWKBLen, CPLFree );
        }
        else if ( eGeomFormat == OSGF_SpatiaLite )
        {
            int     nBLOBLen;
            GByte   *pabySLBLOB;

            ExportSpatiaLiteGeometry( poGeom, nSRSId, wkbNDR,
                                      &pabySLBLOB, &nBLOBLen );
            rc = sqlite3_bind_blob( hInsertStmt, nBindField++, pabySLBLOB, nBLOBLen, CPLFree );
        }
        else
        {
            CPLAssert(0);
        }

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "sqlite3_bind_blob/text() failed:\n  %s", 
                      sqlite3_errmsg(hDB) );
            
            sqlite3_finalize( hInsertStmt );
            return OGRERR_FAILURE;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Bind field values.                                              */
/* -------------------------------------------------------------------- */
    
    for( iField = 0; iField < nFieldCount; iField++ )
    {
        const char *pszRawValue;

        if( !poFeature->IsFieldSet( iField ) )
            continue;

        switch( poFeatureDefn->GetFieldDefn(iField)->GetType() )
        {
            case OFTInteger:
            {
                int nFieldVal = poFeature->GetFieldAsInteger( iField );
                rc = sqlite3_bind_int(hInsertStmt, nBindField++, nFieldVal);
                break;
            }
            
            case OFTReal:
            {
                double dfFieldVal = poFeature->GetFieldAsDouble( iField );
                rc = sqlite3_bind_double(hInsertStmt, nBindField++, dfFieldVal);
                break;
            }
            
            case OFTBinary:
            {
                int nDataLength = 0;
                GByte* pabyData =
                    poFeature->GetFieldAsBinary( iField, &nDataLength );
                rc = sqlite3_bind_blob(hInsertStmt, nBindField++,
                                       pabyData, nDataLength, SQLITE_TRANSIENT);
                break;
            }
            
            default:
            {
                pszRawValue = poFeature->GetFieldAsString( iField );
                rc = sqlite3_bind_text(hInsertStmt, nBindField++,
                                       pszRawValue, -1, SQLITE_TRANSIENT);
                break;
            }
        }
        
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "sqlite3_bind_() for column %s failed:\n  %s", 
                      poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                      sqlite3_errmsg(hDB) );
            
            sqlite3_finalize( hInsertStmt );
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    rc = sqlite3_step( hInsertStmt );

    if( rc != SQLITE_OK && rc != SQLITE_DONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "sqlite3_step() failed:\n  %s", 
                  sqlite3_errmsg(hDB) );
                  
        sqlite3_finalize( hInsertStmt );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Capture the FID/rowid.                                          */
/* -------------------------------------------------------------------- */
    const sqlite_int64 nFID = sqlite3_last_insert_rowid( hDB );
    if(nFID > 0)
    {
        poFeature->SetFID( nFID );
    }

    sqlite3_finalize( hInsertStmt );

    return OGRERR_NONE;
}

