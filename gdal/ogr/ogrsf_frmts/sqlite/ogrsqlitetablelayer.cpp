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
#include "ogr_p.h"
#include <string>

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRSQLiteTableLayer()                         */
/************************************************************************/

OGRSQLiteTableLayer::OGRSQLiteTableLayer( OGRSQLiteDataSource *poDSIn )

{
    poDS = poDSIn;
	
    bSpatialite2D = FALSE;
    bLaunderColumnNames = TRUE;

    iNextShapeId = 0;

    nSRSId = -1;

    poFeatureDefn = NULL;
    pszEscapedTableName = NULL;

    bHasCheckedSpatialIndexTable = FALSE;

    hInsertStmt = NULL;
}

/************************************************************************/
/*                        ~OGRSQLiteTableLayer()                        */
/************************************************************************/

OGRSQLiteTableLayer::~OGRSQLiteTableLayer()

{
    ClearStatement();
    ClearInsertStmt();
    CPLFree(pszEscapedTableName);
}

/************************************************************************/
/*                          ClearInsertStmt()                           */
/************************************************************************/

void OGRSQLiteTableLayer::ClearInsertStmt()
{
    if( hInsertStmt != NULL )
    {
        sqlite3_finalize( hInsertStmt );
        hInsertStmt = NULL;
    }
    osLastInsertStmt = "";
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
                                        int bHasSpatialIndex,
                                        int bHasM, 
                                        int bSpatialiteReadOnly,
                                        int bSpatialiteLoaded,
                                        int iSpatialiteVersion,
                                        int bIsVirtualShapeIn )

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
    this->bHasM = bHasM;
    this->bSpatialiteReadOnly = bSpatialiteReadOnly;
    this->bSpatialiteLoaded = bSpatialiteLoaded;
    this->iSpatialiteVersion = iSpatialiteVersion;
    this->bIsVirtualShape = bIsVirtualShapeIn;

    pszEscapedTableName = CPLStrdup(OGRSQLiteEscape(pszTableName));

    CPLErr eErr;
    sqlite3_stmt *hColStmt = NULL;
    const char *pszSQL;

    if ( eGeomFormat == OSGF_SpatiaLite &&
         bSpatialiteLoaded == TRUE && 
         iSpatialiteVersion < 24 && poDS->GetUpdate() )
    {
    // we need to test version required by Spatialite TRIGGERs 
        hColStmt = NULL;
        pszSQL = CPLSPrintf( "SELECT sql FROM sqlite_master WHERE type = 'trigger' AND tbl_name = '%s' AND sql LIKE '%%RTreeAlign%%'",
            pszEscapedTableName );

        int nRowTriggerCount, nColTriggerCount;
        char **papszTriggerResult, *pszErrMsg;

        rc = sqlite3_get_table( hDB, pszSQL, &papszTriggerResult,
            &nRowTriggerCount, &nColTriggerCount, &pszErrMsg );
        if( nRowTriggerCount >= 1 )
        {
        // obsolete library version not supporting new triggers
        // enforcing ReadOnly mode
            CPLDebug("SQLITE", "Enforcing ReadOnly mode : obsolete library version not supporting new triggers");
            this->bSpatialiteReadOnly = TRUE;
        }

        sqlite3_free_table( papszTriggerResult );
    }
	
    if( poSRS )
        poSRS->Reference();

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    hColStmt = NULL;
    pszSQL = CPLSPrintf( "SELECT _rowid_, * FROM '%s' LIMIT 1",
                                     pszEscapedTableName );

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
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::ResetStatement()

{
    int rc;
    CPLString osSQL;

    ClearStatement();

    iNextShapeId = 0;

    osSQL.Printf( "SELECT _rowid_, * FROM '%s' %s",
                    pszEscapedTableName, 
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

    osSQL.Printf( "SELECT _rowid_, * FROM '%s' WHERE \"%s\" = %ld",
                  pszEscapedTableName, 
                  pszFIDColumn, nFeatureId );

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
/*                        CheckSpatialIndexTable()                      */
/************************************************************************/

int OGRSQLiteTableLayer::CheckSpatialIndexTable()
{
    if (bHasSpatialIndex && !bHasCheckedSpatialIndexTable)
    {
        bHasCheckedSpatialIndexTable = TRUE;
        char **papszResult;
        int nRowCount, nColCount;
        char *pszErrMsg = NULL;

        CPLString osSQL;
        osSQL.Printf("SELECT name FROM sqlite_master "
                    "WHERE name='idx_%s_%s'",
                    pszEscapedTableName, osGeomColumn.c_str());

        int  rc = sqlite3_get_table( poDS->GetDB(), osSQL.c_str(),
                                    &papszResult, &nRowCount,
                                    &nColCount, &pszErrMsg );

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Error: %s",
                    pszErrMsg );
            sqlite3_free( pszErrMsg );
            bHasSpatialIndex = FALSE;
        }
        else
        {
            if (nRowCount != 1)
            {
                bHasSpatialIndex = FALSE;
                CPLDebug("SQLITE", "Count not find idx_%s_%s layer. Disabling spatial index",
                        pszEscapedTableName, osGeomColumn.c_str());
            }

            sqlite3_free_table(papszResult);
        }
    }

    return bHasSpatialIndex;
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

    if( m_poFilterGeom != NULL && CheckSpatialIndexTable() )
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );

        osWHERE.Printf("WHERE ROWID IN ( SELECT pkid FROM 'idx_%s_%s' WHERE "
                        "xmax >= %.12f AND xmin <= %.12f AND ymax >= %.12f AND ymin <= %.12f) ",
                        pszEscapedTableName, osGeomColumn.c_str(),
                        sEnvelope.MinX - 1e-11, sEnvelope.MaxX + 1e-11,
                        sEnvelope.MinY - 1e-11, sEnvelope.MaxY + 1e-11);
    }

    if( m_poFilterGeom != NULL && bSpatialiteLoaded && !bHasSpatialIndex)
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );

        /* A bit inefficient but still faster than OGR filtering */
        osWHERE.Printf("WHERE MBRIntersects(\"%s\", BuildMBR(%.12f, %.12f, %.12f, %.12f, %d)) ",
                       osGeomColumn.c_str(),
                       sEnvelope.MinX - 1e-11, sEnvelope.MinY - 1e-11,
                       sEnvelope.MaxX + 1e-11, sEnvelope.MaxY + 1e-11,
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
    {
        if ( bSpatialiteReadOnly == TRUE)
            return FALSE;
        return poDS->GetUpdate();
    }

    else if( EQUAL(pszCap,OLCDeleteFeature) )
    {
        if ( bSpatialiteReadOnly == TRUE)
            return FALSE;
        return poDS->GetUpdate() && pszFIDColumn != NULL;
    }

    else if( EQUAL(pszCap,OLCCreateField) )
        return poDS->GetUpdate();

    else if( EQUAL(pszCap,OLCDeleteField) )
        return poDS->GetUpdate();

    else if( EQUAL(pszCap,OLCAlterFieldDefn) )
        return poDS->GetUpdate();

    else if( EQUAL(pszCap,OLCReorderFields) )
        return poDS->GetUpdate();

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

    if (m_poFilterGeom != NULL && CheckSpatialIndexTable() &&
        strlen(osQuery) == 0)
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );
        pszSQL = CPLSPrintf("SELECT count(*) FROM 'idx_%s_%s' WHERE "
                            "xmax >= %.12f AND xmin <= %.12f AND ymax >= %.12f AND ymin <= %.12f",
                            pszEscapedTableName, osGeomColumn.c_str(),
                            sEnvelope.MinX - 1e-11, sEnvelope.MaxX + 1e-11,
                            sEnvelope.MinY - 1e-11, sEnvelope.MaxY + 1e-11);
    }
    else
    {
        pszSQL = CPLSPrintf( "SELECT count(*) FROM '%s' %s",
                            pszEscapedTableName, osWHERE.c_str() );
    }

    CPLDebug("SQLITE", "Running %s", pszSQL);

/* -------------------------------------------------------------------- */
/*      Execute.                                                        */
/* -------------------------------------------------------------------- */
    char **papszResult, *pszErrMsg;
    int nRowCount, nColCount;
    int nResult = -1;

    if( sqlite3_get_table( poDS->GetDB(), pszSQL, &papszResult, 
                           &nRowCount, &nColCount, &pszErrMsg ) != SQLITE_OK )
        return -1;

    if( nRowCount == 1 && nColCount == 1 )
        nResult = atoi(papszResult[1]);

    sqlite3_free_table( papszResult );

    return nResult;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if (CheckSpatialIndexTable())
    {
        const char* pszSQL;

        pszSQL = CPLSPrintf("SELECT MIN(xmin), MIN(ymin), MAX(xmax), MAX(ymax) FROM 'idx_%s_%s'",
                            pszEscapedTableName, osGeomColumn.c_str());

        CPLDebug("SQLITE", "Running %s", pszSQL);

/* -------------------------------------------------------------------- */
/*      Execute.                                                        */
/* -------------------------------------------------------------------- */
        char **papszResult, *pszErrMsg;
        int nRowCount, nColCount;

        if( sqlite3_get_table( poDS->GetDB(), pszSQL, &papszResult,
                               &nRowCount, &nColCount, &pszErrMsg ) != SQLITE_OK )
            return OGRSQLiteLayer::GetExtent(psExtent, bForce);

        OGRErr eErr = OGRERR_FAILURE;

        if( nRowCount == 1 && nColCount == 4 &&
            papszResult[4+0] != NULL &&
            papszResult[4+1] != NULL &&
            papszResult[4+2] != NULL &&
            papszResult[4+3] != NULL)
        {
            psExtent->MinX = atof(papszResult[4+0]);
            psExtent->MinY = atof(papszResult[4+1]);
            psExtent->MaxX = atof(papszResult[4+2]);
            psExtent->MaxY = atof(papszResult[4+3]);
            eErr = OGRERR_NONE;
        }

        sqlite3_free_table( papszResult );

        if (eErr == OGRERR_NONE)
            return eErr;
    }

    return OGRSQLiteLayer::GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                      OGRFieldTypeToSQliteType()                      */
/************************************************************************/

static const char* OGRFieldTypeToSQliteType( OGRFieldType eType )
{
    if( eType == OFTInteger )
        return "INTEGER";
    else if( eType == OFTReal )
        return "FLOAT";
    else if( eType == OFTBinary )
        return "BLOB";
    else
        return "VARCHAR";
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::CreateField( OGRFieldDefn *poFieldIn, 
                                         int bApproxOK )

{
    OGRFieldDefn        oField( poFieldIn );

    ResetReading();

    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create fields on a read-only layer.");
        return OGRERR_FAILURE;
    }

    ClearInsertStmt();

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

    /* ADD COLUMN only avaliable since sqlite 3.1.3 */
    if (CSLTestBoolean(CPLGetConfigOption("OGR_SQLITE_USE_ADD_COLUMN", "YES")) &&
        sqlite3_libversion_number() > 3 * 1000000 + 1 * 1000 + 3)
    {
        int rc;
        char *pszErrMsg = NULL;
        sqlite3 *hDB = poDS->GetDB();
        CPLString osCommand;

        osCommand.Printf("ALTER TABLE '%s' ADD COLUMN '%s' %s",
                        pszEscapedTableName,
                        oField.GetNameRef(),
                        OGRFieldTypeToSQliteType(oField.GetType()));

    #ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
    #endif

        rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to add field %s to table %s:\n %s",
                      oField.GetNameRef(), poFeatureDefn->GetName(), 
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            return OGRERR_FAILURE;
        }
    }
    else
    {
        OGRErr eErr = AddColumnAncientMethod(oField);
        if (eErr != OGRERR_NONE)
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Add the field to the OGRFeatureDefn.                            */
/* -------------------------------------------------------------------- */
    int iNewField;
    int iNextOrdinal = 3; /* _rowid_ is 1, OGC_FID is 2 */

    if( poFeatureDefn->GetGeomType() != wkbNone )
    {
        iNextOrdinal++;
    }

    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(iField);

        // we already added OGC_FID so don't do it again
        if( EQUAL(poFldDefn->GetNameRef(),"OGC_FID") )
            continue;

        iNextOrdinal++;
    }

    poFeatureDefn->AddFieldDefn( &oField );

    iNewField = poFeatureDefn->GetFieldCount() - 1;
    panFieldOrdinals = (int *)
        CPLRealloc(panFieldOrdinals, (iNewField+1) * sizeof(int) );
    panFieldOrdinals[iNewField] = iNextOrdinal;

    return OGRERR_NONE;
}

/************************************************************************/
/*                     InitFieldListForRecrerate()                      */
/************************************************************************/

void OGRSQLiteTableLayer::InitFieldListForRecrerate(char* & pszNewFieldList,
                                                    char* & pszFieldListForSelect,
                                                    int nExtraSpace)
{
    int iField, nFieldListLen = 100 + nExtraSpace;

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        nFieldListLen +=
            strlen(poFeatureDefn->GetFieldDefn(iField)->GetNameRef()) + 50;
    }

    pszFieldListForSelect = (char *) CPLCalloc(1,nFieldListLen);
    pszNewFieldList = (char *) CPLCalloc(1,nFieldListLen);

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    sprintf( pszFieldListForSelect, "%s", pszFIDColumn ? pszFIDColumn : "OGC_FID" );
    sprintf( pszNewFieldList, "%s INTEGER PRIMARY KEY",pszFIDColumn ? pszFIDColumn : "OGC_FID" );

    if( poFeatureDefn->GetGeomType() != wkbNone )
    {
        strcat( pszFieldListForSelect, "," );
        strcat( pszNewFieldList, "," );

        strcat( pszFieldListForSelect, osGeomColumn );
        strcat( pszNewFieldList, osGeomColumn );

        if ( eGeomFormat == OSGF_WKT )
            strcat( pszNewFieldList, " VARCHAR" );
        else
            strcat( pszNewFieldList, " BLOB" );
    }
}

/************************************************************************/
/*                       AddColumnAncientMethod()                       */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::AddColumnAncientMethod( OGRFieldDefn& oField)
{
    
/* -------------------------------------------------------------------- */
/*      How much space do we need for the list of fields.               */
/* -------------------------------------------------------------------- */
    int iField;
    char *pszOldFieldList, *pszNewFieldList;

    InitFieldListForRecrerate(pszNewFieldList, pszOldFieldList,
                              strlen( oField.GetNameRef() ));

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */

    int iNextOrdinal = 3; /* _rowid_ is 1, OGC_FID is 2 */

    if( poFeatureDefn->GetGeomType() != wkbNone )
    {
        iNextOrdinal++;
    }

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(iField);

        // we already added OGC_FID so don't do it again
        if( EQUAL(poFldDefn->GetNameRef(),pszFIDColumn ? pszFIDColumn : "OGC_FID") )
            continue;

        sprintf( pszOldFieldList+strlen(pszOldFieldList), 
                 ", \"%s\"", poFldDefn->GetNameRef() );

        sprintf( pszNewFieldList+strlen(pszNewFieldList), 
                 ", '%s' %s", poFldDefn->GetNameRef(),
                 OGRFieldTypeToSQliteType(poFldDefn->GetType()) );

        iNextOrdinal++;
    }

/* -------------------------------------------------------------------- */
/*      Add the new field.                                              */
/* -------------------------------------------------------------------- */

    sprintf( pszNewFieldList+strlen(pszNewFieldList), 
             ", '%s' %s", oField.GetNameRef(),
             OGRFieldTypeToSQliteType(oField.GetType()) );

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
                   pszEscapedTableName );

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
                                       pszEscapedTableName ),
                           NULL, NULL, &pszErrMsg );


/* -------------------------------------------------------------------- */
/*      Drop the original table, and recreate with new field.           */
/* -------------------------------------------------------------------- */
    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB, 
                           CPLSPrintf( "DROP TABLE '%s'", 
                                       pszEscapedTableName ),
                           NULL, NULL, &pszErrMsg );

    if( rc == SQLITE_OK )
    {
        const char *pszCmd = 
            CPLSPrintf( "CREATE TABLE '%s' (%s)", 
                        pszEscapedTableName,
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
                                       pszEscapedTableName,
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

    return OGRERR_NONE;
}

/************************************************************************/
/*                           RecreateTable()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::RecreateTable(const char* pszFieldListForSelect,
                                          const char* pszNewFieldList,
                                          const char* pszGenericErrorMessage)
{
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
                   pszEscapedTableName );

    int nRowTriggerIndexCount, nColTriggerIndexCount;
    char **papszTriggerIndexResult = NULL;
    rc = sqlite3_get_table( hDB, osSQL.c_str(), &papszTriggerIndexResult,
                            &nRowTriggerIndexCount, &nColTriggerIndexCount, &pszErrMsg );

/* -------------------------------------------------------------------- */
/*      Make a backup of the table.                                     */
/* -------------------------------------------------------------------- */

    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB,
                       CPLSPrintf( "CREATE TABLE t1_back(%s)",
                                   pszNewFieldList ),
                       NULL, NULL, &pszErrMsg );

    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB,
                           CPLSPrintf( "INSERT INTO t1_back SELECT %s FROM '%s'",
                                       pszFieldListForSelect,
                                       pszEscapedTableName ),
                           NULL, NULL, &pszErrMsg );


/* -------------------------------------------------------------------- */
/*      Drop the original table                                         */
/* -------------------------------------------------------------------- */
    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB,
                           CPLSPrintf( "DROP TABLE '%s'",
                                       pszEscapedTableName ),
                           NULL, NULL, &pszErrMsg );

/* -------------------------------------------------------------------- */
/*      Rename backup table as new table                                */
/* -------------------------------------------------------------------- */
    if( rc == SQLITE_OK )
    {
        const char *pszCmd =
            CPLSPrintf( "ALTER TABLE t1_back RENAME TO '%s'",
                        pszEscapedTableName);
        rc = sqlite3_exec( hDB, pszCmd,
                           NULL, NULL, &pszErrMsg );
    }

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

        return OGRERR_NONE;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s:\n %s",
                  pszGenericErrorMessage,
                  pszErrMsg );
        sqlite3_free( pszErrMsg );

        poDS->SoftRollback();

        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                             DeleteField()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::DeleteField( int iFieldToDelete )
{
    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't delete fields on a read-only layer.");
        return OGRERR_FAILURE;
    }

    if (iFieldToDelete < 0 || iFieldToDelete >= poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    ResetReading();

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    int iField;
    char *pszNewFieldList, *pszFieldListForSelect;
    InitFieldListForRecrerate(pszNewFieldList, pszFieldListForSelect);

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(iField);

        if (iField == iFieldToDelete)
            continue;

        sprintf( pszFieldListForSelect+strlen(pszFieldListForSelect),
                 ", \"%s\"", poFldDefn->GetNameRef() );

        sprintf( pszNewFieldList+strlen(pszNewFieldList),
                 ", '%s' %s", poFldDefn->GetNameRef(),
                 OGRFieldTypeToSQliteType(poFldDefn->GetType()) );
    }

/* -------------------------------------------------------------------- */
/*      Recreate table.                                                 */
/* -------------------------------------------------------------------- */
    CPLString osErrorMsg;
    osErrorMsg.Printf("Failed to remove field %s from table %s",
                  poFeatureDefn->GetFieldDefn(iFieldToDelete)->GetNameRef(),
                  poFeatureDefn->GetName());

    OGRErr eErr = RecreateTable(pszFieldListForSelect,
                                pszNewFieldList,
                                osErrorMsg.c_str());

    CPLFree( pszFieldListForSelect );
    CPLFree( pszNewFieldList );

    if (eErr != OGRERR_NONE)
        return eErr;

/* -------------------------------------------------------------------- */
/*      Finish                                                          */
/* -------------------------------------------------------------------- */
    int iNextOrdinal = 3; /* _rowid_ is 1, OGC_FID is 2 */

    if( poFeatureDefn->GetGeomType() != wkbNone )
    {
        iNextOrdinal++;
    }

    int iNewField = 0;
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if (iField == iFieldToDelete)
            continue;

        panFieldOrdinals[iNewField ++] = iNextOrdinal++;
    }

    return poFeatureDefn->DeleteFieldDefn( iFieldToDelete );
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::AlterFieldDefn( int iFieldToAlter, OGRFieldDefn* poNewFieldDefn, int nFlags )
{
    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't alter field definition on a read-only layer.");
        return OGRERR_FAILURE;
    }

    if (iFieldToAlter < 0 || iFieldToAlter >= poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    ClearInsertStmt();
    ResetReading();

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    int iField;
    char *pszNewFieldList, *pszFieldListForSelect;
    InitFieldListForRecrerate(pszNewFieldList, pszFieldListForSelect,
                              strlen(poNewFieldDefn->GetNameRef()));

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(iField);

        sprintf( pszFieldListForSelect+strlen(pszFieldListForSelect),
                 ", \"%s\"", poFldDefn->GetNameRef() );

        if (iField == iFieldToAlter)
        {
            sprintf( pszNewFieldList+strlen(pszNewFieldList),
                    ", '%s' %s",
                    (nFlags & ALTER_NAME_FLAG) ? poNewFieldDefn->GetNameRef() :
                                                 poFldDefn->GetNameRef(),
                    OGRFieldTypeToSQliteType((nFlags & ALTER_TYPE_FLAG) ?
                            poNewFieldDefn->GetType() : poFldDefn->GetType()) );
        }
        else
        {
            sprintf( pszNewFieldList+strlen(pszNewFieldList),
                    ", '%s' %s", poFldDefn->GetNameRef(),
                    OGRFieldTypeToSQliteType(poFldDefn->GetType()) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Recreate table.                                                 */
/* -------------------------------------------------------------------- */
    CPLString osErrorMsg;
    osErrorMsg.Printf("Failed to alter field %s from table %s",
                  poFeatureDefn->GetFieldDefn(iFieldToAlter)->GetNameRef(),
                  poFeatureDefn->GetName());

    OGRErr eErr = RecreateTable(pszFieldListForSelect,
                                pszNewFieldList,
                                osErrorMsg.c_str());

    CPLFree( pszFieldListForSelect );
    CPLFree( pszNewFieldList );

    if (eErr != OGRERR_NONE)
        return eErr;

/* -------------------------------------------------------------------- */
/*      Finish                                                          */
/* -------------------------------------------------------------------- */

    OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(iFieldToAlter);

    if (nFlags & ALTER_TYPE_FLAG)
        poFieldDefn->SetType(poNewFieldDefn->GetType());
    if (nFlags & ALTER_NAME_FLAG)
        poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
    if (nFlags & ALTER_WIDTH_PRECISION_FLAG)
    {
        poFieldDefn->SetWidth(poNewFieldDefn->GetWidth());
        poFieldDefn->SetPrecision(poNewFieldDefn->GetPrecision());
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::ReorderFields( int* panMap )
{
    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't reorder fields on a read-only layer.");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    OGRErr eErr = OGRCheckPermutation(panMap, poFeatureDefn->GetFieldCount());
    if (eErr != OGRERR_NONE)
        return eErr;

    ClearInsertStmt();
    ResetReading();

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    int iField;
    char *pszNewFieldList, *pszFieldListForSelect;
    InitFieldListForRecrerate(pszNewFieldList, pszFieldListForSelect);

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(panMap[iField]);

        sprintf( pszFieldListForSelect+strlen(pszFieldListForSelect),
                 ", \"%s\"", poFldDefn->GetNameRef() );

        sprintf( pszNewFieldList+strlen(pszNewFieldList),
                ", '%s' %s", poFldDefn->GetNameRef(),
                OGRFieldTypeToSQliteType(poFldDefn->GetType()) );
    }

/* -------------------------------------------------------------------- */
/*      Recreate table.                                                 */
/* -------------------------------------------------------------------- */
    CPLString osErrorMsg;
    osErrorMsg.Printf("Failed to reorder fields from table %s",
                  poFeatureDefn->GetName());

    eErr = RecreateTable(pszFieldListForSelect,
                                pszNewFieldList,
                                osErrorMsg.c_str());

    CPLFree( pszFieldListForSelect );
    CPLFree( pszNewFieldList );

    if (eErr != OGRERR_NONE)
        return eErr;

/* -------------------------------------------------------------------- */
/*      Finish                                                          */
/* -------------------------------------------------------------------- */

    return poFeatureDefn->ReorderFieldDefns( panMap );
}

/************************************************************************/
/*                             BindValues()                             */
/************************************************************************/

/* the bBindNullValues is set to TRUE by SetFeature() for UPDATE statements, */
/* and to FALSE by CreateFeature() for INSERT statements; */

OGRErr OGRSQLiteTableLayer::BindValues( OGRFeature *poFeature,
                                        sqlite3_stmt* hStmt,
                                        int bBindNullValues )
{
    int rc;
    sqlite3 *hDB = poDS->GetDB();

/* -------------------------------------------------------------------- */
/*      Bind the geometry                                               */
/* -------------------------------------------------------------------- */
    int nBindField = 1;

    if( osGeomColumn.size() != 0 &&
        eGeomFormat != OSGF_FGF )
    {
        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        if ( poGeom != NULL )
        {
            if ( eGeomFormat == OSGF_WKT )
            {
                char *pszWKT = NULL;
                poGeom->exportToWkt( &pszWKT );
                rc = sqlite3_bind_text( hStmt, nBindField++, pszWKT, -1, CPLFree );
            }
            else if( eGeomFormat == OSGF_WKB )
            {
                int nWKBLen = poGeom->WkbSize();
                GByte *pabyWKB = (GByte *) CPLMalloc(nWKBLen + 1);

                poGeom->exportToWkb( wkbNDR, pabyWKB );
                rc = sqlite3_bind_blob( hStmt, nBindField++, pabyWKB, nWKBLen, CPLFree );
            }
            else if ( eGeomFormat == OSGF_SpatiaLite )
            {
                int     nBLOBLen;
                GByte   *pabySLBLOB;

                ExportSpatiaLiteGeometry( poGeom, nSRSId, wkbNDR, bHasM,
                                        bSpatialite2D, bUseComprGeom, &pabySLBLOB, &nBLOBLen );
                rc = sqlite3_bind_blob( hStmt, nBindField++, pabySLBLOB,
                                        nBLOBLen, CPLFree );
            }
            else
            {
                rc = SQLITE_OK;
                CPLAssert(0);
            }
        }
        else
        {
            if (bBindNullValues)
                rc = sqlite3_bind_null( hStmt, nBindField++ );
            else
                rc = SQLITE_OK;
        }

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "sqlite3_bind_blob/text() failed:\n  %s",
                      sqlite3_errmsg(hDB) );
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Bind field values.                                              */
/* -------------------------------------------------------------------- */
    int iField;
    int nFieldCount = poFeatureDefn->GetFieldCount();
    for( iField = 0; iField < nFieldCount; iField++ )
    {
        const char *pszRawValue;

        if( !poFeature->IsFieldSet( iField ) )
        {
            if (bBindNullValues)
                rc = sqlite3_bind_null( hStmt, nBindField++ );
            else
                rc = SQLITE_OK;
        }
        else
        {
            switch( poFeatureDefn->GetFieldDefn(iField)->GetType() )
            {
                case OFTInteger:
                {
                    int nFieldVal = poFeature->GetFieldAsInteger( iField );
                    rc = sqlite3_bind_int(hStmt, nBindField++, nFieldVal);
                    break;
                }

                case OFTReal:
                {
                    double dfFieldVal = poFeature->GetFieldAsDouble( iField );
                    rc = sqlite3_bind_double(hStmt, nBindField++, dfFieldVal);
                    break;
                }

                case OFTBinary:
                {
                    int nDataLength = 0;
                    GByte* pabyData =
                        poFeature->GetFieldAsBinary( iField, &nDataLength );
                    rc = sqlite3_bind_blob(hStmt, nBindField++,
                                        pabyData, nDataLength, SQLITE_TRANSIENT);
                    break;
                }

                default:
                {
                    pszRawValue = poFeature->GetFieldAsString( iField );
                    rc = sqlite3_bind_text(hStmt, nBindField++,
                                        pszRawValue, -1, SQLITE_TRANSIENT);
                    break;
                }
            }
        }

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "sqlite3_bind_() for column %s failed:\n  %s",
                      poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                      sqlite3_errmsg(hDB) );
            return OGRERR_FAILURE;
        }
    }

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

    if (bSpatialiteReadOnly || !poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't update feature on a read-only layer.");
        return OGRERR_FAILURE;
    }

    sqlite3 *hDB = poDS->GetDB();
    CPLString      osCommand;
    int            bNeedComma = FALSE;

    ResetReading();

/* -------------------------------------------------------------------- */
/*      Form the UPDATE command.                                        */
/* -------------------------------------------------------------------- */
    osCommand += CPLSPrintf( "UPDATE '%s' SET ", pszEscapedTableName );

/* -------------------------------------------------------------------- */
/*      Add geometry field name.                                        */
/* -------------------------------------------------------------------- */
    if( osGeomColumn.size() != 0 &&
        eGeomFormat != OSGF_FGF )
    {
        osCommand += "\"";
        osCommand += osGeomColumn;
        osCommand += "\" = ?";

        bNeedComma = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Add field names.                                                */
/* -------------------------------------------------------------------- */
    int iField;
    int nFieldCount = poFeatureDefn->GetFieldCount();

    for( iField = 0; iField < nFieldCount; iField++ )
    {
        if( bNeedComma )
            osCommand += ",";

        osCommand += "\"";
        osCommand +=poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
        osCommand += "\" = ?";

        bNeedComma = TRUE;
    }

    if (!bNeedComma)
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Merge final command.                                            */
/* -------------------------------------------------------------------- */
    osCommand += " WHERE \"";
    osCommand += pszFIDColumn;
    osCommand += CPLSPrintf("\" = %ld", poFeature->GetFID());

/* -------------------------------------------------------------------- */
/*      Prepare the statement.                                          */
/* -------------------------------------------------------------------- */
    int rc;
    sqlite3_stmt *hUpdateStmt;

#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "prepare(%s)", osCommand.c_str() );
#endif

    rc = sqlite3_prepare( hDB, osCommand, -1, &hUpdateStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "In SetFeature(): sqlite3_prepare(%s):\n  %s",
                  osCommand.c_str(), sqlite3_errmsg(hDB) );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Bind values.                                                   */
/* -------------------------------------------------------------------- */
    OGRErr eErr = BindValues( poFeature, hUpdateStmt, TRUE );
    if (eErr != OGRERR_NONE)
    {
        sqlite3_finalize( hUpdateStmt );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Execute the update.                                             */
/* -------------------------------------------------------------------- */
    rc = sqlite3_step( hUpdateStmt );

    if( rc != SQLITE_OK && rc != SQLITE_DONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_step() failed:\n  %s",
                  sqlite3_errmsg(hDB) );

        sqlite3_finalize( hUpdateStmt );
        return OGRERR_FAILURE;
    }

    sqlite3_finalize( hUpdateStmt );

    return OGRERR_NONE;
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

    if (bSpatialiteReadOnly || !poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create feature on a read-only layer.");
        return OGRERR_FAILURE;
    }

    ResetReading();

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    osCommand += CPLSPrintf( "INSERT INTO '%s' (", pszEscapedTableName );

/* -------------------------------------------------------------------- */
/*      Add FID if we have a cleartext FID column.                      */
/* -------------------------------------------------------------------- */
    if( pszFIDColumn != NULL // && !EQUAL(pszFIDColumn,"OGC_FID") 
        && poFeature->GetFID() != OGRNullFID )
    {
        osCommand += "\"";
        osCommand += pszFIDColumn;
        osCommand += "\"";

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

        osCommand += "\"";
        osCommand += osGeomColumn;
        osCommand += "\"";

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

        osCommand += "\"";
        osCommand +=poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
        osCommand += "\"";

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

    if (hInsertStmt == NULL ||
        osCommand != osLastInsertStmt)
    {
    #ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "prepare(%s)", osCommand.c_str() );
    #endif

        ClearInsertStmt();
        osLastInsertStmt = osCommand;

        rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hInsertStmt, NULL );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "In CreateFeature(): sqlite3_prepare(%s):\n  %s",
                    osCommand.c_str(), sqlite3_errmsg(hDB) );

            ClearInsertStmt();
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Bind values.                                                   */
/* -------------------------------------------------------------------- */
    OGRErr eErr = BindValues( poFeature, hInsertStmt, FALSE );
    if (eErr != OGRERR_NONE)
    {
        sqlite3_reset( hInsertStmt );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    rc = sqlite3_step( hInsertStmt );

    if( rc != SQLITE_OK && rc != SQLITE_DONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "sqlite3_step() failed:\n  %s (%d)", 
                  sqlite3_errmsg(hDB), rc );
        sqlite3_reset( hInsertStmt );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Capture the FID/rowid.                                          */
/* -------------------------------------------------------------------- */
    const sqlite_int64 nFID = sqlite3_last_insert_rowid( hDB );
    if(nFID > 0)
    {
        poFeature->SetFID( (long)nFID ); /* Possible truncation if nFID is 64bit */
    }

    sqlite3_reset( hInsertStmt );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::DeleteFeature( long nFID )

{
    CPLString      osSQL;
    int            rc;
    char          *pszErrMsg = NULL;

    if( pszFIDColumn == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't delete feature on a layer without FID column.");
        return OGRERR_FAILURE;
    }

    if (bSpatialiteReadOnly || !poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't delete feature on a read-only layer.");
        return OGRERR_FAILURE;
    }

    ResetReading();

    osSQL.Printf( "DELETE FROM '%s' WHERE \"%s\" = %ld",
                  pszEscapedTableName,
                  pszFIDColumn, nFID );

    CPLDebug( "OGR_SQLITE", "exec(%s)", osSQL.c_str() );

    rc = sqlite3_exec( poDS->GetDB(), osSQL, NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "In DeleteFeature(): sqlite3_exec(%s):\n  %s",
                    osSQL.c_str(), pszErrMsg );
        sqlite3_free( pszErrMsg );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}
