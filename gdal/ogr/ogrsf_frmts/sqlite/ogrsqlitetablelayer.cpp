/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_time.h"
#include <string>

#define UNSUPPORTED_OP_READ_ONLY "%s : unsupported operation on a read-only datasource."

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRSQLiteTableLayer()                         */
/************************************************************************/

OGRSQLiteTableLayer::OGRSQLiteTableLayer( OGRSQLiteDataSource *poDSIn )

{
    poDS = poDSIn;

    bLaunderColumnNames = TRUE;

    /* SpatiaLite v.2.4.0 (or any subsequent) is required
       to support 2.5D: if an obsolete version of the library
       is found we'll unconditionally activate 2D casting mode.
    */
    bSpatialite2D = poDS->GetSpatialiteVersionNumber() < 24;

    iNextShapeId = 0;

    poFeatureDefn = NULL;
    pszTableName = NULL;
    pszEscapedTableName = NULL;

    bHasCheckedSpatialIndexTable = FALSE;
    bDeferedSpatialIndexCreation = FALSE;

    hInsertStmt = NULL;
    bHasDefaultValue = FALSE;

    eGeomType = wkbUnknown;
    bLayerDefnError = FALSE;

    bStatisticsNeedsToBeFlushed = FALSE;
    bCachedExtentIsValid = FALSE;
    nFeatureCount = -1;

    eGeomFormat = OSGF_None;
    pszGeomCol = NULL;
    nSRSId = UNINITIALIZED_SRID;
    poSRS = NULL;

    int bDisableInsertTriggers = CSLTestBoolean(CPLGetConfigOption(
                            "OGR_SQLITE_DISABLE_INSERT_TRIGGERS", "YES"));
    bHasCheckedTriggers = !bDisableInsertTriggers;
}

/************************************************************************/
/*                        ~OGRSQLiteTableLayer()                        */
/************************************************************************/

OGRSQLiteTableLayer::~OGRSQLiteTableLayer()

{
    ClearStatement();
    ClearInsertStmt();

    // Restore temporarily disabled triggers
    char* pszErrMsg = NULL;
    for(size_t i = 0; i < aosDisabledTriggers.size(); i++ )
    {
        CPLDebug("SQLite", "Restoring trigger %s", aosDisabledTriggers[i].first.c_str());
        // This may fail since CreateSpatialIndex() reinstalls triggers, so
        // don't check result
        sqlite3_exec( poDS->GetDB(), aosDisabledTriggers[i].second.c_str(), NULL, NULL, &pszErrMsg );
        if( pszErrMsg )
            sqlite3_free( pszErrMsg );
        pszErrMsg = NULL;
    }
 
    // Update geometry_columns_time
    if( aosDisabledTriggers.size() != 0 && pszGeomCol != NULL )
    {
        char* pszSQL3 = sqlite3_mprintf(
            "UPDATE geometry_columns_time SET last_insert = strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ', 'now') "
            "WHERE Lower(f_table_name) = Lower('%q') AND Lower(f_geometry_column) = Lower('%q')",
            pszTableName, pszGeomCol);
        sqlite3_exec( poDS->GetDB(), pszSQL3, NULL, NULL, &pszErrMsg );
        if( pszErrMsg )
            sqlite3_free( pszErrMsg );
        pszErrMsg = NULL;
    }

    CPLFree(pszGeomCol);
    if( poSRS != NULL )
    {
        poSRS->Release();
        poSRS = NULL;
    }

    CPLFree(pszTableName);
    CPLFree(pszEscapedTableName);
}

/************************************************************************/
/*                     CreateSpatialIndexIfNecessary()                  */
/************************************************************************/

void OGRSQLiteTableLayer::CreateSpatialIndexIfNecessary()
{
    if( bDeferedSpatialIndexCreation )
    {
        CreateSpatialIndex();
    }
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
                                        int bMustIncludeGeomColName,
                                        OGRwkbGeometryType eGeomType,
                                        const char *pszGeomFormat,
                                        OGRSpatialReference *poSRS,
                                        int nSRSId,
                                        int bHasSpatialIndex,
                                        int bHasM,
                                        int bIsVirtualShapeIn )
{
    /* int rc; */
    sqlite3 *hDB = poDS->GetDB();

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

    if( nSRSId == UNINITIALIZED_SRID )
        nSRSId = poDS->GetUndefinedSRID();

    this->poSRS = poSRS;
    this->nSRSId = nSRSId;
    this->bHasSpatialIndex = bHasSpatialIndex;
    this->bHasM = bHasM;
    this->bIsVirtualShape = bIsVirtualShapeIn;
    this->pszTableName = CPLStrdup(pszTableName);
    this->pszGeomCol = (pszGeomCol) ? CPLStrdup(pszGeomCol) : NULL;
    this->eGeomType = eGeomType;

    if( bMustIncludeGeomColName )
        osLayerName.Printf("%s(%s)", pszTableName, pszGeomCol);
    else
        osLayerName = pszTableName;
    SetDescription( osLayerName );

    pszEscapedTableName = CPLStrdup(OGRSQLiteEscape(pszTableName));

    // sqlite3_stmt *hColStmt = NULL;
    const char *pszSQL;

    if ( eGeomFormat == OSGF_SpatiaLite &&
         poDS->IsSpatialiteLoaded() &&
         poDS->GetSpatialiteVersionNumber() < 24 && poDS->GetUpdate() )
    {
        // we need to test version required by Spatialite TRIGGERs
        // hColStmt = NULL;
        pszSQL = CPLSPrintf( "SELECT sql FROM sqlite_master WHERE type = 'trigger' AND tbl_name = '%s' AND sql LIKE '%%RTreeAlign%%'",
            pszEscapedTableName );

        int nRowTriggerCount, nColTriggerCount;
        char **papszTriggerResult, *pszErrMsg;

        /* rc = */ sqlite3_get_table( hDB, pszSQL, &papszTriggerResult,
            &nRowTriggerCount, &nColTriggerCount, &pszErrMsg );
        if( nRowTriggerCount >= 1 )
        {
        // obsolete library version not supporting new triggers
        // enforcing ReadOnly mode
            CPLDebug("SQLITE", "Enforcing ReadOnly mode : obsolete library version not supporting new triggers");
            poDS->SetUpdate(FALSE);
        }

        sqlite3_free_table( papszTriggerResult );
    }

    if( poSRS )
        poSRS->Reference();

    LoadStatistics();

    return CE_None;
}

/************************************************************************/
/*                               GetName()                              */
/************************************************************************/

const char* OGRSQLiteTableLayer::GetName()
{
    return osLayerName.c_str();
}

/************************************************************************/
/*                         EstablishFeatureDefn()                       */
/************************************************************************/

CPLErr OGRSQLiteTableLayer::EstablishFeatureDefn()
{
    sqlite3 *hDB = poDS->GetDB();
    int rc;
    const char *pszSQL;
    sqlite3_stmt *hColStmt = NULL;

/* -------------------------------------------------------------------- */
/*      Check if there are default values.                              */
/* -------------------------------------------------------------------- */

    char **papszResult;
    int nRowCount, nColCount;
    char *pszErrMsg = NULL;
    char* pszSQL3 = sqlite3_mprintf("PRAGMA table_info('%q')", pszTableName);
    rc = sqlite3_get_table( hDB, pszSQL3, &papszResult, &nRowCount,
                            &nColCount, &pszErrMsg );
    sqlite3_free( pszSQL3 );
    if( rc != SQLITE_OK )
    {
        sqlite3_free( pszErrMsg );
    }
    else
    {
        if( nColCount == 6 )
        {
            for(int i=0;i<nRowCount;i++)
            {
                if( papszResult[(i+1)*6+4] != NULL )
                    bHasDefaultValue = TRUE;
            }
        }
        sqlite3_free_table(papszResult);
    }

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */

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
    CPLFree( pszFIDColumn );
    pszFIDColumn = CPLStrdup(OGRSQLiteParamsUnquote(sqlite3_column_name( hColStmt, 0 )));

/* -------------------------------------------------------------------- */
/*      Collect the rest of the fields.                                 */
/* -------------------------------------------------------------------- */
    BuildFeatureDefn( osLayerName, hColStmt, pszGeomCol, poDS->GetGeomColsForTable(pszTableName) );
    sqlite3_finalize( hColStmt );

/* -------------------------------------------------------------------- */
/*      Set the properties of the geometry column.                      */
/* -------------------------------------------------------------------- */
    if( poFeatureDefn->GetGeomFieldCount() != 0 )
    {
        poFeatureDefn->SetGeomType( eGeomType );
        OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->myGetGeomFieldDefn(0);
        if( pszGeomCol == NULL )
            pszGeomCol = CPLStrdup( poGeomFieldDefn->GetNameRef() );
        poGeomFieldDefn->nSRSId = nSRSId;
        if( eGeomFormat != OSGF_None )
            poGeomFieldDefn->eGeomFormat = eGeomFormat;
        poGeomFieldDefn->SetSpatialRef(poSRS);
    }

    return CE_None;
}

/************************************************************************/
/*                         RecomputeOrdinals()                          */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::RecomputeOrdinals()
{
    sqlite3 *hDB = poDS->GetDB();
    int rc;
    const char *pszSQL;
    sqlite3_stmt *hColStmt = NULL;
/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */

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

    int    nRawColumns = sqlite3_column_count( hColStmt );

    CPLFree(panFieldOrdinals);
    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * poFeatureDefn->GetFieldCount() );
    int nCountFieldOrdinals = 0;
    int nCountGeomFieldOrdinals = 0;
    iFIDCol = -1;

    int iCol;
    for( iCol = 0; iCol < nRawColumns; iCol++ )
    {
        CPLString osName =
            OGRSQLiteParamsUnquote(sqlite3_column_name( hColStmt, iCol ));
        int nIdx = poFeatureDefn->GetFieldIndex(osName);
        if( nIdx >= 0 )
        {
            panFieldOrdinals[nIdx] = iCol;
            nCountFieldOrdinals ++;
        }
        else
        {
            nIdx = poFeatureDefn->GetGeomFieldIndex(osName);
            if( nIdx >= 0 )
            {
                OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                    poFeatureDefn->myGetGeomFieldDefn(nIdx);
                poGeomFieldDefn->iCol = iCol;
                nCountGeomFieldOrdinals ++;
            }
            else if( pszFIDColumn != NULL && strcmp(osName, pszFIDColumn) == 0 )
                iFIDCol = iCol;
        }
    }
    CPLAssert(nCountFieldOrdinals == poFeatureDefn->GetFieldCount() );
    CPLAssert(nCountGeomFieldOrdinals == poFeatureDefn->GetGeomFieldCount() );
    CPLAssert(pszFIDColumn == NULL || iFIDCol > 0 );

    sqlite3_finalize( hColStmt );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn* OGRSQLiteTableLayer::GetLayerDefn()
{
    if (poFeatureDefn)
        return poFeatureDefn;

    EstablishFeatureDefn();

    if (poFeatureDefn == NULL)
    {
        bLayerDefnError = TRUE;

        poFeatureDefn = new OGRSQLiteFeatureDefn( osLayerName );
        poFeatureDefn->Reference();
    }

    return poFeatureDefn;
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


//#ifdef HAVE_SQLITE3_PREPARE_V2
//    rc = sqlite3_prepare_v2( poDS->GetDB(), osSQL, osSQL.size(),
//                  &hStmt, NULL );
//#else
    rc = sqlite3_prepare( poDS->GetDB(), osSQL, osSQL.size(),
		          &hStmt, NULL );
//#endif

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
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSQLiteTableLayer::GetNextFeature()

{
    if (HasLayerDefnError())
        return NULL;

    return OGRSQLiteLayer::GetNextFeature();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRSQLiteTableLayer::GetFeature( long nFeatureId )

{
    if (HasLayerDefnError())
        return NULL;

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
                  OGRSQLiteEscape(pszFIDColumn).c_str(), nFeatureId );

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
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

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
    if (HasSpatialIndex() && !bHasCheckedSpatialIndexTable)
    {
        bHasCheckedSpatialIndexTable = TRUE;
        char **papszResult;
        int nRowCount, nColCount;
        char *pszErrMsg = NULL;

        CPLString osSQL;

        /* This will ensure that RTree support is available */
        osSQL.Printf("SELECT pkid FROM 'idx_%s_%s' WHERE xmax > 0 AND xmin < 0 AND ymax > 0 AND ymin < 0",
                     pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str());

        int  rc = sqlite3_get_table( poDS->GetDB(), osSQL.c_str(),
                                    &papszResult, &nRowCount,
                                    &nColCount, &pszErrMsg );

        if( rc != SQLITE_OK )
        {
            CPLDebug("SQLITE", "Count not find or use idx_%s_%s layer (%s). Disabling spatial index",
                        pszEscapedTableName, pszGeomCol, pszErrMsg);
            sqlite3_free( pszErrMsg );
            bHasSpatialIndex = FALSE;
        }
        else
        {
            sqlite3_free_table(papszResult);
        }
    }

    return bHasSpatialIndex;
}

/************************************************************************/
/*                         HasFastSpatialFilter()                       */
/************************************************************************/

int OGRSQLiteTableLayer::HasFastSpatialFilter(int iGeomCol)
{
    OGRPolygon oFakePoly;
    const char* pszWKT = "POLYGON((0 0,0 1,1 1,1 0,0 0))";
    oFakePoly.importFromWkt((char**) &pszWKT);
    CPLString    osSpatialWhere = GetSpatialWhere(iGeomCol, &oFakePoly);
    return osSpatialWhere.find("ROWID") == 0;
}

/************************************************************************/
/*                           GetSpatialWhere()                          */
/************************************************************************/

CPLString OGRSQLiteTableLayer::GetSpatialWhere(int iGeomCol,
                                               OGRGeometry* poFilterGeom)
{
    if( !poDS->IsSpatialiteDB() || poFeatureDefn == NULL ||
        iGeomCol < 0 || iGeomCol >= poFeatureDefn->GetGeomFieldCount() )
        return "";

    if( poFilterGeom != NULL && CheckSpatialIndexTable() )
    {
        return FormatSpatialFilterFromRTree(poFilterGeom, "ROWID",
            pszEscapedTableName,
            OGRSQLiteEscape(poFeatureDefn->GetGeomFieldDefn(iGeomCol)->GetNameRef()).c_str());
    }

    if( poFilterGeom != NULL &&
        poDS->IsSpatialiteLoaded() && !bHasSpatialIndex )
    {
        return FormatSpatialFilterFromMBR(poFilterGeom,
            OGRSQLiteEscapeName(poFeatureDefn->GetGeomFieldDefn(iGeomCol)->GetNameRef()).c_str());
    }

    return "";
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

    CPLString osSpatialWHERE = GetSpatialWhere(m_iGeomFieldFilter,
                                               m_poFilterGeom);
    if (osSpatialWHERE.size() != 0)
    {
        osWHERE = "WHERE ";
        osWHERE += osSpatialWHERE;
    }

    if( osQuery.size() > 0 )
    {
        if( osWHERE.size() == 0 )
        {
            osWHERE = "WHERE ";
            osWHERE += osQuery;
        }
        else	
        {
            osWHERE += " AND (";
            osWHERE += osQuery;
            osWHERE += ")";
        }
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteTableLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap,OLCFastFeatureCount))
        return m_poFilterGeom == NULL || pszGeomCol == NULL ||
               HasSpatialIndex();

    else if (EQUAL(pszCap,OLCFastSpatialFilter))
        return HasSpatialIndex();

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return bCachedExtentIsValid;

    else if( EQUAL(pszCap,OLCRandomRead) )
        return pszFIDColumn != NULL;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
    {
        return poDS->GetUpdate();
    }

    else if( EQUAL(pszCap,OLCDeleteFeature) )
    {
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

    else if( EQUAL(pszCap,OLCCurveGeometries) )
        return poDS->TestCapability(ODsCCurveGeometries);

    else 
        return OGRSQLiteLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRSQLiteTableLayer::GetFeatureCount( int bForce )

{
    if (HasLayerDefnError())
        return 0;

    if( !TestCapability(OLCFastFeatureCount) )
        return OGRSQLiteLayer::GetFeatureCount( bForce );

    if (nFeatureCount >= 0 && m_poFilterGeom == NULL &&
        osQuery.size() == 0 )
    {
        return nFeatureCount;
    }

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
                            pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str(),
                            sEnvelope.MinX - 1e-11,
                            sEnvelope.MaxX + 1e-11,
                            sEnvelope.MinY - 1e-11,
                            sEnvelope.MaxY + 1e-11);
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
    {
        nResult = atoi(papszResult[1]);

        if( m_poFilterGeom == NULL && osQuery.size() == 0 )
        {
            nFeatureCount = nResult;
            bStatisticsNeedsToBeFlushed = TRUE;
        }
    }

    sqlite3_free_table( papszResult );

    return nResult;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if (GetGeomType() == wkbNone)
        return OGRERR_FAILURE;

    if (bCachedExtentIsValid)
    {
        memcpy(psExtent, &oCachedExtent, sizeof(oCachedExtent));
        return OGRERR_NONE;
    }

    if (CheckSpatialIndexTable() &&
        !CSLTestBoolean(CPLGetConfigOption("OGR_SQLITE_EXACT_EXTENT", "NO")))
    {
        const char* pszSQL;

        pszSQL = CPLSPrintf("SELECT MIN(xmin), MIN(ymin), MAX(xmax), MAX(ymax) FROM 'idx_%s_%s'",
                            pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str());

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
            psExtent->MinX = CPLAtof(papszResult[4+0]);
            psExtent->MinY = CPLAtof(papszResult[4+1]);
            psExtent->MaxX = CPLAtof(papszResult[4+2]);
            psExtent->MaxY = CPLAtof(papszResult[4+3]);
            eErr = OGRERR_NONE;

            if( m_poFilterGeom == NULL && osQuery.size() == 0 )
            {
                bCachedExtentIsValid = TRUE;
                bStatisticsNeedsToBeFlushed = TRUE;
                memcpy(&oCachedExtent, psExtent, sizeof(oCachedExtent));
            }
        }

        sqlite3_free_table( papszResult );

        if (eErr == OGRERR_NONE)
            return eErr;
    }

    OGRErr eErr = OGRSQLiteLayer::GetExtent(psExtent, bForce);
    if( eErr == OGRERR_NONE && m_poFilterGeom == NULL && osQuery.size() == 0 )
    {
        bCachedExtentIsValid = TRUE;
        bStatisticsNeedsToBeFlushed = TRUE;
        memcpy(&oCachedExtent, psExtent, sizeof(oCachedExtent));
    }
    return eErr;
}

/************************************************************************/
/*                  OGRSQLiteFieldDefnToSQliteFieldDefn()               */
/************************************************************************/

CPLString OGRSQLiteFieldDefnToSQliteFieldDefn( OGRFieldDefn* poFieldDefn,
                                               int bSQLiteDialectInternalUse )
{
    switch( poFieldDefn->GetType() )
    {
        case OFTInteger:
            if (poFieldDefn->GetSubType() == OFSTBoolean) 
                return "INTEGER_BOOLEAN";
            else if (poFieldDefn->GetSubType() == OFSTInt16 ) 
                return "INTEGER_INT16";
            else
                return "INTEGER";
            break;
        case OFTReal:
            if (bSQLiteDialectInternalUse && poFieldDefn->GetSubType() == OFSTFloat32) 
                return "FLOAT_FLOAT32";
            else
                return "FLOAT";
            break;
        case OFTBinary : return "BLOB"; break;
        case OFTString :
        {
            if( poFieldDefn->GetWidth() > 0 )
                return CPLSPrintf("VARCHAR(%d)", poFieldDefn->GetWidth());
            else
                return "VARCHAR";
            break;
        }
        case OFTDateTime: return "TIMESTAMP"; break;
        case OFTDate    : return "DATE"; break;
        case OFTTime    : return "TIME"; break;
        case OFTIntegerList:
            if (bSQLiteDialectInternalUse )
                return "INTEGERLIST";
            else
                return "VARCHAR";
            break;
        case OFTRealList:
            if (bSQLiteDialectInternalUse )
                return "REALLIST";
            else
                return "VARCHAR";
            break;
        case OFTStringList:
            if (bSQLiteDialectInternalUse )
                return "STRINGLIST";
            else
                return "VARCHAR";
            break;
        default         : return "VARCHAR"; break;
    }
}

/************************************************************************/
/*                    FieldDefnToSQliteFieldDefn()                      */
/************************************************************************/

CPLString OGRSQLiteTableLayer::FieldDefnToSQliteFieldDefn( OGRFieldDefn* poFieldDefn )
{
    CPLString osRet = OGRSQLiteFieldDefnToSQliteFieldDefn(poFieldDefn, FALSE);
    if( poFieldDefn->GetType() == OFTString &&
        CSLFindString(papszCompressedColumns, poFieldDefn->GetNameRef()) >= 0 )
        osRet += "_deflate";

    return osRet;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::CreateField( OGRFieldDefn *poFieldIn,
                                         CPL_UNUSED int bApproxOK )
{
    OGRFieldDefn        oField( poFieldIn );

    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    ResetReading();

    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "CreateField");
        return OGRERR_FAILURE;
    }

    ClearInsertStmt();
    
    if( poDS->IsSpatialiteDB() && EQUAL( oField.GetNameRef(), "ROWID") )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "In a Spatialite DB, a 'ROWID' column that is not the integer "
                 "primary key can corrupt spatial index. "
                 "See https://www.gaia-gis.it/fossil/libspatialite/wiki?name=Shadowed+ROWID+issues");
    }

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


    if( (oField.GetType() == OFTTime || oField.GetType() == OFTDate ||
         oField.GetType() == OFTDateTime) &&
        !(CSLTestBoolean(
            CPLGetConfigOption("OGR_SQLITE_ENABLE_DATETIME", "YES"))) )
    {
        oField.SetType(OFTString);
    }

    /* ADD COLUMN only avaliable since sqlite 3.1.3 */
    if (CSLTestBoolean(CPLGetConfigOption("OGR_SQLITE_USE_ADD_COLUMN", "YES")) &&
        sqlite3_libversion_number() > 3 * 1000000 + 1 * 1000 + 3)
    {
        int rc;
        char *pszErrMsg = NULL;
        sqlite3 *hDB = poDS->GetDB();
        CPLString osCommand;

        CPLString osFieldType(FieldDefnToSQliteFieldDefn(&oField));
        osCommand.Printf("ALTER TABLE '%s' ADD COLUMN '%s' %s",
                        pszEscapedTableName,
                        OGRSQLiteEscape(oField.GetNameRef()).c_str(),
                        osFieldType.c_str());

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
    poFeatureDefn->AddFieldDefn( &oField );

    RecomputeOrdinals();

    return OGRERR_NONE;
}

/************************************************************************/
/*                     InitFieldListForRecrerate()                      */
/************************************************************************/

void OGRSQLiteTableLayer::InitFieldListForRecrerate(char* & pszNewFieldList,
                                                    char* & pszFieldListForSelect,
                                                    int nExtraSpace)
{
    int iField, nFieldListLen = 100 + 2 * nExtraSpace;

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        nFieldListLen +=
            2 * strlen(poFeatureDefn->GetFieldDefn(iField)->GetNameRef()) + 50;
    }

    nFieldListLen += 50 + (pszFIDColumn ? 2 * strlen(pszFIDColumn) : strlen("OGC_FID"));
    if( poFeatureDefn->GetGeomType() != wkbNone )
    {
        nFieldListLen += 50 + 2 * strlen(pszGeomCol);
    }

    pszFieldListForSelect = (char *) CPLCalloc(1,nFieldListLen);
    pszNewFieldList = (char *) CPLCalloc(1,nFieldListLen);

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    sprintf( pszFieldListForSelect, "\"%s\"", pszFIDColumn ? OGRSQLiteEscapeName(pszFIDColumn).c_str() : "OGC_FID" );
    sprintf( pszNewFieldList, "\"%s\" INTEGER PRIMARY KEY",pszFIDColumn ? OGRSQLiteEscapeName(pszFIDColumn).c_str() : "OGC_FID" );

    if( poFeatureDefn->GetGeomType() != wkbNone )
    {
        strcat( pszFieldListForSelect, "," );
        strcat( pszNewFieldList, "," );

        strcat( pszFieldListForSelect, "\"");
        strcat( pszFieldListForSelect, OGRSQLiteEscapeName(pszGeomCol) );
        strcat( pszFieldListForSelect, "\"");
        
        strcat( pszNewFieldList, "\"");
        strcat( pszNewFieldList, OGRSQLiteEscapeName(pszGeomCol) );
        strcat( pszNewFieldList, "\"");

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
                 ", \"%s\"", OGRSQLiteEscapeName(poFldDefn->GetNameRef()).c_str() );

        sprintf( pszNewFieldList+strlen(pszNewFieldList), 
                 ", '%s' %s", OGRSQLiteEscape(poFldDefn->GetNameRef()).c_str(),
                 FieldDefnToSQliteFieldDefn(poFldDefn).c_str() );

        iNextOrdinal++;
    }

/* -------------------------------------------------------------------- */
/*      Add the new field.                                              */
/* -------------------------------------------------------------------- */

    sprintf( pszNewFieldList+strlen(pszNewFieldList), 
             ", '%s' %s", OGRSQLiteEscape(oField.GetNameRef()).c_str(),
             FieldDefnToSQliteFieldDefn(&oField).c_str() );

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
    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteField");
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
                 ", \"%s\"", OGRSQLiteEscapeName(poFldDefn->GetNameRef()).c_str() );

        sprintf( pszNewFieldList+strlen(pszNewFieldList),
                 ", '%s' %s", OGRSQLiteEscape(poFldDefn->GetNameRef()).c_str(),
                 FieldDefnToSQliteFieldDefn(poFldDefn).c_str() );
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
    eErr = poFeatureDefn->DeleteFieldDefn( iFieldToDelete );

    RecomputeOrdinals();

    return eErr;
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::AlterFieldDefn( int iFieldToAlter, OGRFieldDefn* poNewFieldDefn, int nFlags )
{
    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "AlterFieldDefn");
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
                 ", \"%s\"", OGRSQLiteEscapeName(poFldDefn->GetNameRef()).c_str() );

        if (iField == iFieldToAlter)
        {
            OGRFieldDefn oTmpFieldDefn(poFldDefn);
            if( (nFlags & ALTER_NAME_FLAG) )
                oTmpFieldDefn.SetName(poNewFieldDefn->GetNameRef());
            if( (nFlags & ALTER_TYPE_FLAG) )
                oTmpFieldDefn.SetType(poNewFieldDefn->GetType());
            if (nFlags & ALTER_WIDTH_PRECISION_FLAG)
            {
                oTmpFieldDefn.SetWidth(poNewFieldDefn->GetWidth());
                oTmpFieldDefn.SetPrecision(poNewFieldDefn->GetPrecision());
            }

            sprintf( pszNewFieldList+strlen(pszNewFieldList),
                    ", '%s' %s",
                    OGRSQLiteEscape(oTmpFieldDefn.GetNameRef()).c_str(),
                    FieldDefnToSQliteFieldDefn(&oTmpFieldDefn).c_str() );
            if ( (nFlags & ALTER_NAME_FLAG) &&
                 oTmpFieldDefn.GetType() == OFTString &&
                 CSLFindString(papszCompressedColumns, poFldDefn->GetNameRef()) >= 0 )
            {
                sprintf( pszNewFieldList+strlen(pszNewFieldList), "_deflate");
            }
        }
        else
        {
            sprintf( pszNewFieldList+strlen(pszNewFieldList),
                    ", '%s' %s", OGRSQLiteEscape(poFldDefn->GetNameRef()).c_str(),
                    FieldDefnToSQliteFieldDefn(poFldDefn).c_str() );
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
    {
        int iIdx;
        if( poNewFieldDefn->GetType() != OFTString &&
            (iIdx = CSLFindString(papszCompressedColumns,
                                  poFieldDefn->GetNameRef())) >= 0 )
        {
            papszCompressedColumns = CSLRemoveStrings(papszCompressedColumns,
                                                      iIdx, 1, NULL);
        }
        poFieldDefn->SetType(poNewFieldDefn->GetType());
    }
    if (nFlags & ALTER_NAME_FLAG)
    {
        int iIdx;
        if( (iIdx = CSLFindString(papszCompressedColumns,
                                  poFieldDefn->GetNameRef())) >= 0 )
        {
            CPLFree(papszCompressedColumns[iIdx]);
            papszCompressedColumns[iIdx] =
                CPLStrdup(poNewFieldDefn->GetNameRef());
        }
        poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
    }
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
    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "ReorderFields");
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
                 ", \"%s\"", OGRSQLiteEscapeName(poFldDefn->GetNameRef()).c_str() );

        sprintf( pszNewFieldList+strlen(pszNewFieldList),
                ", '%s' %s", OGRSQLiteEscape(poFldDefn->GetNameRef()).c_str(),
                FieldDefnToSQliteFieldDefn(poFldDefn).c_str() );
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

    eErr = poFeatureDefn->ReorderFieldDefns( panMap );

    RecomputeOrdinals();

    return eErr;
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

    if( poFeatureDefn->GetGeomFieldCount() != 0 &&
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

                if( nSRSId == UNINITIALIZED_SRID )
                    nSRSId = poDS->GetUndefinedSRID();

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

                case OFTDateTime:
                {
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZ;
                    poFeature->GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                                &nHour, &nMinute, &nSecond, &nTZ);
                    char szBuffer[64];
                    sprintf(szBuffer, "%04d-%02d-%02dT%02d:%02d:%02d",
                            nYear, nMonth, nDay, nHour, nMinute, nSecond);
                    rc = sqlite3_bind_text(hStmt, nBindField++,
                                           szBuffer, -1, SQLITE_TRANSIENT);
                    break;
                }

                case OFTDate:
                {
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZ;
                    poFeature->GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                                &nHour, &nMinute, &nSecond, &nTZ);
                    char szBuffer[64];
                    sprintf(szBuffer, "%04d-%02d-%02d", nYear, nMonth, nDay);
                    rc = sqlite3_bind_text(hStmt, nBindField++,
                                           szBuffer, -1, SQLITE_TRANSIENT);
                    break;
                }

                case OFTTime:
                {
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZ;
                    poFeature->GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                                &nHour, &nMinute, &nSecond, &nTZ);
                    char szBuffer[64];
                    sprintf(szBuffer, "%02d:%02d:%02d", nHour, nMinute, nSecond);
                    rc = sqlite3_bind_text(hStmt, nBindField++,
                                           szBuffer, -1, SQLITE_TRANSIENT);
                    break;
                }

                case OFTStringList:
                {
                    char** papszValues = poFeature->GetFieldAsStringList( iField );
                    CPLString osValue;
                    osValue += CPLSPrintf("(%d:", CSLCount(papszValues));
                    for(int i=0; papszValues[i] != NULL; i++)
                    {
                        if( i != 0 )
                            osValue += ",";
                        osValue += papszValues[i];
                    }
                    osValue += ")";
                    rc = sqlite3_bind_text(hStmt, nBindField++,
                                               osValue.c_str(), -1, SQLITE_TRANSIENT);
                    break;
                }

                default:
                {
                    pszRawValue = poFeature->GetFieldAsString( iField );
                    if( CSLFindString(papszCompressedColumns,
                                      poFeatureDefn->GetFieldDefn(iField)->GetNameRef()) >= 0 )
                    {
                        size_t nBytesOut = 0;
                        void* pOut = CPLZLibDeflate( pszRawValue,
                                                     strlen(pszRawValue), -1,
                                                     NULL, 0,
                                                     &nBytesOut );
                        if( pOut != NULL )
                        {
                            rc = sqlite3_bind_blob(hStmt, nBindField++,
                                                   pOut,
                                                   nBytesOut,
                                                   CPLFree);
                        }
                        else
                            rc = SQLITE_ERROR;
                    }
                    else
                    {
                        rc = sqlite3_bind_text(hStmt, nBindField++,
                                               pszRawValue, -1, SQLITE_TRANSIENT);
                    }
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
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::ISetFeature( OGRFeature *poFeature )

{
    if (HasLayerDefnError())
        return OGRERR_FAILURE;

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

    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "SetFeature");
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
    if( poFeatureDefn->GetGeomFieldCount() != 0 &&
        eGeomFormat != OSGF_FGF )
    {
        osCommand += "\"";
        osCommand += OGRSQLiteEscapeName(pszGeomCol);
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
        osCommand += OGRSQLiteEscapeName(poFeatureDefn->GetFieldDefn(iField)->GetNameRef());
        osCommand += "\" = ?";

        bNeedComma = TRUE;
    }

    if (!bNeedComma)
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Merge final command.                                            */
/* -------------------------------------------------------------------- */
    osCommand += " WHERE \"";
    osCommand += OGRSQLiteEscapeName(pszFIDColumn);
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

    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    if( bCachedExtentIsValid &&
        poGeom != NULL && !poGeom->IsEmpty() )
    {
        OGREnvelope sGeomEnvelope;
        poGeom->getEnvelope(&sGeomEnvelope);
        oCachedExtent.Merge(sGeomEnvelope);
    }
    bStatisticsNeedsToBeFlushed = TRUE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          AreTriggersSimilar                          */
/************************************************************************/

static int AreTriggersSimilar(const char* pszExpectedTrigger,
                              const char* pszTriggerSQL)
{
    int i;
    for(i=0; pszTriggerSQL[i] != '\0' && pszExpectedTrigger[i] != '\0'; i++)
    {
        if( pszTriggerSQL[i] == pszExpectedTrigger[i] )
            continue;
        if( pszTriggerSQL[i] == '\n' && pszExpectedTrigger[i] == ' ' )
            continue;
        if( pszTriggerSQL[i] == ' ' && pszExpectedTrigger[i] == '\n' )
            continue;
        return FALSE;
    }
    return pszTriggerSQL[i] == '\0' && pszExpectedTrigger[i] == '\0';
}

/************************************************************************/
/*                          ICreateFeature()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::ICreateFeature( OGRFeature *poFeature )

{
    sqlite3 *hDB = poDS->GetDB();
    CPLString      osCommand;
    int            bNeedComma = FALSE;

    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "CreateFeature");
        return OGRERR_FAILURE;
    }

    // For speed-up, disable Spatialite triggers that :
    // * check the geometry type
    // * update the last_insert columns in geometry_columns_time and the spatial index
    // We do that only if there's no spatial index currently active
    // We'll check ourselves the first constraint and update last_insert
    // at layer closing
    if( !bHasCheckedTriggers &&
        (bDeferedSpatialIndexCreation || !bHasSpatialIndex) &&
        poDS->HasSpatialite4Layout() && pszGeomCol &&
        !bHasM )
    {
        bHasCheckedTriggers = TRUE;

        char* pszErrMsg = NULL;

        // Backup INSERT ON triggers
        int nRowCount = 0, nColCount = 0;
        char **papszResult = NULL;
        char* pszSQL3 = sqlite3_mprintf("SELECT name, sql FROM sqlite_master WHERE "
            "tbl_name = '%q' AND type = 'trigger' AND (name LIKE 'ggi_%%' OR name LIKE 'tmi_%%')",
            pszTableName);
        sqlite3_get_table( poDS->GetDB(),
                           pszSQL3,
                           &papszResult,
                           &nRowCount, &nColCount, &pszErrMsg );
        sqlite3_free(pszSQL3);

        if( pszErrMsg )
            sqlite3_free( pszErrMsg );
        pszErrMsg = NULL;

        for(int i=0;i<nRowCount;i++)
        {
            const char* pszTriggerName = papszResult[2*(i+1)+0];
            const char* pszTriggerSQL = papszResult[2*(i+1)+1];
            if( pszTriggerName!= NULL && pszTriggerSQL != NULL )
            {
                const char* pszExpectedTrigger = 0;
                if( strncmp(pszTriggerName, "ggi_", 4) == 0 )
                {
                    pszExpectedTrigger = CPLSPrintf(
                    "CREATE TRIGGER \"ggi_%s_%s\" BEFORE INSERT ON \"%s\" "
                    "FOR EACH ROW BEGIN "
                    "SELECT RAISE(ROLLBACK, '%s.%s violates Geometry constraint [geom-type or SRID not allowed]') "
                    "WHERE (SELECT geometry_type FROM geometry_columns "
                    "WHERE Lower(f_table_name) = Lower('%s') AND Lower(f_geometry_column) = Lower('%s') "
                    "AND GeometryConstraints(NEW.\"%s\", geometry_type, srid) = 1) IS NULL; "
                    "END",
                    pszTableName, pszGeomCol, pszTableName,
                    pszTableName, pszGeomCol,
                    pszTableName, pszGeomCol,
                    pszGeomCol);
                }
                else if( strncmp(pszTriggerName, "tmi_", 4) == 0 )
                {
                    pszExpectedTrigger = CPLSPrintf(
                    "CREATE TRIGGER \"tmi_%s_%s\" AFTER INSERT ON \"%s\" "
                    "FOR EACH ROW BEGIN "
                    "UPDATE geometry_columns_time SET last_insert = strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ', 'now') "
                    "WHERE Lower(f_table_name) = Lower('%s') AND Lower(f_geometry_column) = Lower('%s'); "
                    "END",
                    pszTableName, pszGeomCol, pszTableName,
                    pszTableName, pszGeomCol);
                }
                /* Cannot happen due to the tests that lead to that code path */
                /* that check there's no spatial index active */
                /* A further potential optimization would be to rebuild the spatial index */
                /* afterwards... */
                /*else if( strncmp(pszTriggerName, "gii_", 4) == 0 )
                {
                    pszExpectedTrigger = CPLSPrintf(
                    "CREATE TRIGGER \"gii_%s_%s\" AFTER INSERT ON \"%s\" "
                    "FOR EACH ROW BEGIN "
                    "UPDATE geometry_columns_time SET last_insert = strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ', 'now') "
                    "WHERE Lower(f_table_name) = Lower('%s') AND Lower(f_geometry_column) = Lower('%s'); "
                    "DELETE FROM \"idx_%s_%s\" WHERE pkid=NEW.ROWID; "
                    "SELECT RTreeAlign('idx_%s_%s', NEW.ROWID, NEW.\"%s\"); "
                    "END",
                    pszTableName, pszGeomCol, pszTableName,
                    pszTableName, pszGeomCol,
                    pszTableName, pszGeomCol,
                    pszTableName, pszGeomCol, pszGeomCol);
                }*/

                if( AreTriggersSimilar(pszExpectedTrigger, pszTriggerSQL) )
                {
                    // And drop them
                    pszSQL3 = sqlite3_mprintf("DROP TRIGGER %s", pszTriggerName);
                    int rc = sqlite3_exec( poDS->GetDB(), pszSQL3, NULL, NULL, &pszErrMsg );
                    if( rc != SQLITE_OK )
                        CPLDebug("SQLITE", "Error %s", pszErrMsg ? pszErrMsg : "");
                    else
                    {
                        CPLDebug("SQLite", "Dropping trigger %s", pszTriggerName);
                        aosDisabledTriggers.push_back(std::pair<CPLString,CPLString>(pszTriggerName, pszTriggerSQL));
                    }
                    sqlite3_free(pszSQL3);
                    if( pszErrMsg )
                        sqlite3_free( pszErrMsg );
                    pszErrMsg = NULL;
                }
                else
                {
                    CPLDebug("SQLite", "Cannot drop %s trigger. Doesn't match expected definition",
                             pszTriggerName);
                }
            }
        }

        sqlite3_free_table( papszResult );
    }

    ResetReading();

    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    
    if( aosDisabledTriggers.size() != 0 && eGeomType != wkbUnknown && poGeom != NULL )
    {
        if( poGeom->getGeometryType() != eGeomType )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot insert feature with geometry of type %s%s. Type %s%s expected",
                      OGRToOGCGeomType(poGeom->getGeometryType()),
                      (wkbFlatten(poGeom->getGeometryType()) != poGeom->getGeometryType()) ? "Z" :"",
                      OGRToOGCGeomType(eGeomType),
                      (wkbFlatten(eGeomType) != eGeomType) ? "Z": "" );
            return OGRERR_FAILURE;
        }
    }

    int bReuseStmt = FALSE;
    if( hInsertStmt == NULL || poFeature->GetFID() != OGRNullFID || bHasDefaultValue )
    {
        CPLString      osValues;

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
            osCommand += OGRSQLiteEscapeName(pszFIDColumn);
            osCommand += "\"";

            osValues += CPLSPrintf( "%ld", poFeature->GetFID() );
            bNeedComma = TRUE;
        }

/* -------------------------------------------------------------------- */
/*      Add geometry.                                                   */
/* -------------------------------------------------------------------- */
        if( poFeatureDefn->GetGeomFieldCount() != 0 &&
            (!bHasDefaultValue || poGeom != NULL) &&
            eGeomFormat != OSGF_FGF )
        {

            if( bNeedComma )
            {
                osCommand += ",";
                osValues += ",";
            }

            osCommand += "\"";
            osCommand += OGRSQLiteEscapeName(pszGeomCol);
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
            if( bHasDefaultValue && !poFeature->IsFieldSet( iField ) )
                continue;

            if( bNeedComma )
            {
                osCommand += ",";
                osValues += ",";
            }

            osCommand += "\"";
            osCommand += OGRSQLiteEscapeName(poFeatureDefn->GetFieldDefn(iField)->GetNameRef());
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

        if (bNeedComma == FALSE)
            osCommand = CPLSPrintf( "INSERT INTO '%s' DEFAULT VALUES", pszEscapedTableName );
    }
    else
        bReuseStmt = TRUE;

/* -------------------------------------------------------------------- */
/*      Prepare the statement.                                          */
/* -------------------------------------------------------------------- */
    int rc;

    if( !bReuseStmt && (hInsertStmt == NULL || osCommand != osLastInsertStmt) )
    {
    #ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "prepare(%s)", osCommand.c_str() );
    #endif

        ClearInsertStmt();
        if( poFeature->GetFID() == OGRNullFID )
            osLastInsertStmt = osCommand;

#ifdef HAVE_SQLITE3_PREPARE_V2
        rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hInsertStmt, NULL );
#else
        rc = sqlite3_prepare( hDB, osCommand, -1, &hInsertStmt, NULL );
#endif
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
    OGRErr eErr = BindValues( poFeature, hInsertStmt, !bHasDefaultValue );
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

    if( (bCachedExtentIsValid || nFeatureCount == 0) &&
        poGeom != NULL && !poGeom->IsEmpty() )
    {
        OGREnvelope sGeomEnvelope;
        poGeom->getEnvelope(&sGeomEnvelope);
        oCachedExtent.Merge(sGeomEnvelope);
        bCachedExtentIsValid = TRUE;
        bStatisticsNeedsToBeFlushed = TRUE;
    }
    if( nFeatureCount >= 0 )
    {
        bStatisticsNeedsToBeFlushed = TRUE;
        nFeatureCount ++;
    }

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

    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if( pszFIDColumn == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't delete feature on a layer without FID column.");
        return OGRERR_FAILURE;
    }

    if (!poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteFeature");
        return OGRERR_FAILURE;
    }

    ResetReading();

    osSQL.Printf( "DELETE FROM '%s' WHERE \"%s\" = %ld",
                  pszEscapedTableName,
                  OGRSQLiteEscapeName(pszFIDColumn).c_str(), nFID );

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

    int nChanged = sqlite3_changes( poDS->GetDB() );

    if( nChanged == 1 )
    {
        bCachedExtentIsValid = FALSE;
        nFeatureCount --;
        bStatisticsNeedsToBeFlushed = TRUE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CreateSpatialIndex()                         */
/************************************************************************/

int OGRSQLiteTableLayer::CreateSpatialIndex()
{
    CPLString osCommand;

    if( pszGeomCol == NULL )
        return FALSE;

    osCommand.Printf("SELECT CreateSpatialIndex('%s', '%s')",
                     pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str());

    char* pszErrMsg = NULL;
    sqlite3 *hDB = poDS->GetDB();
#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif
    int rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Unable to create spatial index:\n%s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    bHasSpatialIndex = TRUE;
    return TRUE;
}

/************************************************************************/
/*                           HasSpatialIndex()                          */
/************************************************************************/

int OGRSQLiteTableLayer::HasSpatialIndex()
{
    if( bDeferedSpatialIndexCreation )
    {
        bDeferedSpatialIndexCreation = FALSE;
        bHasSpatialIndex = CreateSpatialIndex();
    }

    return bHasSpatialIndex;
}

/************************************************************************/
/*                          InitFeatureCount()                          */
/************************************************************************/

void OGRSQLiteTableLayer::InitFeatureCount()
{
    nFeatureCount = 0;
    bStatisticsNeedsToBeFlushed = TRUE;
}

/************************************************************************/
/*                 InvalidateCachedFeatureCountAndExtent()              */
/************************************************************************/

void OGRSQLiteTableLayer::InvalidateCachedFeatureCountAndExtent()
{
    nFeatureCount = -1;
    bCachedExtentIsValid = FALSE;
    bStatisticsNeedsToBeFlushed = TRUE;
}

/************************************************************************/
/*                     DoStatisticsNeedToBeFlushed()                    */
/************************************************************************/

int OGRSQLiteTableLayer::DoStatisticsNeedToBeFlushed()
{
    return bStatisticsNeedsToBeFlushed &&
           poDS->IsSpatialiteDB() &&
           poDS->IsSpatialiteLoaded();
}

/************************************************************************/
/*                     ForceStatisticsToBeFlushed()                     */
/************************************************************************/

void OGRSQLiteTableLayer::ForceStatisticsToBeFlushed()
{
    bStatisticsNeedsToBeFlushed = TRUE;
}

/************************************************************************/
/*                         AreStatisticsValid()                         */
/************************************************************************/

int OGRSQLiteTableLayer::AreStatisticsValid()
{
    return nFeatureCount >= 0;
}

/************************************************************************/
/*                     LoadStatisticsSpatialite4DB()                    */
/************************************************************************/

void OGRSQLiteTableLayer::LoadStatisticsSpatialite4DB()
{
    if( pszGeomCol == NULL )
        return;

    CPLString osSQL;
    CPLString osLastEvtDate;
    osSQL.Printf("SELECT MAX(last_insert, last_update, last_delete) FROM geometry_columns_time WHERE "
                 "f_table_name = '%s' AND f_geometry_column = '%s'",
                 pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str());

    sqlite3 *hDB = poDS->GetDB();
    int nRowCount = 0, nColCount = 0;
    char **papszResult = NULL;

    sqlite3_get_table( hDB, osSQL.c_str(), &papszResult,
                       &nRowCount, &nColCount, NULL );

    /* Make it a Unix timestamp */
    int nYear, nMonth, nDay, nHour, nMinute;
    float fSecond;
    if( nRowCount == 1 && nColCount == 1 && papszResult[1] != NULL &&
        sscanf( papszResult[1], "%04d-%02d-%02dT%02d:%02d:%f",
                &nYear, &nMonth, &nDay, &nHour, &nMinute, &fSecond ) == 6 )
    {
        osLastEvtDate = papszResult[1];
    }

    sqlite3_free_table( papszResult );
    papszResult = NULL;

    if( osLastEvtDate.size() == 0 )
        return;

    osSQL.Printf("SELECT last_verified, row_count, extent_min_x, extent_min_y, "
                 "extent_max_x, extent_max_y FROM geometry_columns_statistics WHERE "
                 "f_table_name = '%s' AND f_geometry_column = '%s'",
                 pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str());

    nRowCount = 0;
    nColCount = 0;
    sqlite3_get_table( hDB, osSQL.c_str(), &papszResult,
                       &nRowCount, &nColCount, NULL );

    if( nRowCount == 1 && nColCount == 6 && papszResult[6] != NULL &&
        sscanf( papszResult[6], "%04d-%02d-%02dT%02d:%02d:%f",
                &nYear, &nMonth, &nDay, &nHour, &nMinute, &fSecond ) == 6 )
    {
        CPLString osLastVerified(papszResult[6]);

        /* Check that the information in geometry_columns_statistics is more */
        /* recent than geometry_columns_time */
        if( osLastVerified.compare(osLastEvtDate) > 0 )
        {
            char **papszRow = papszResult + 6;
            const char* pszRowCount = papszRow[1];
            const char* pszMinX = papszRow[2];
            const char* pszMinY = papszRow[3];
            const char* pszMaxX = papszRow[4];
            const char* pszMaxY = papszRow[5];

            CPLDebug("SQLITE",  "Loading statistics for %s", pszTableName);

            if( pszRowCount != NULL )
            {
                nFeatureCount = (GIntBig) CPLScanUIntBig( pszRowCount, 32 );
                if( nFeatureCount == 0)
                {
                    nFeatureCount = -1;
                    pszMinX = NULL;
                }
                else
                {
                    CPLDebug("SQLite", "Layer %s feature count : " CPL_FRMT_GIB,
                                pszTableName, nFeatureCount);
                }
            }

            if( pszMinX != NULL && pszMinY != NULL &&
                pszMaxX != NULL && pszMaxY != NULL )
            {
                bCachedExtentIsValid = TRUE;
                oCachedExtent.MinX = CPLAtof(pszMinX);
                oCachedExtent.MinY = CPLAtof(pszMinY);
                oCachedExtent.MaxX = CPLAtof(pszMaxX);
                oCachedExtent.MaxY = CPLAtof(pszMaxY);
                CPLDebug("SQLite", "Layer %s extent : %s,%s,%s,%s",
                            pszTableName, pszMinX,pszMinY,pszMaxX,pszMaxY);
            }
        }
    }

    sqlite3_free_table( papszResult );
    papszResult = NULL;
}

/************************************************************************/
/*                          LoadStatistics()                            */
/************************************************************************/

void OGRSQLiteTableLayer::LoadStatistics()
{
    if( !poDS->IsSpatialiteDB() || !poDS->IsSpatialiteLoaded() || pszGeomCol == NULL )
        return;

    if( poDS->HasSpatialite4Layout() )
    {
        LoadStatisticsSpatialite4DB();
        return;
    }

    GIntBig nFileTimestamp = poDS->GetFileTimestamp();
    if( nFileTimestamp == 0 )
        return;

    /* Find the most recent event in the 'spatialite_history' that is */
    /* a UpdateLayerStatistics event on all tables and geometry columns */
    CPLString osSQL;
    osSQL.Printf("SELECT MAX(timestamp) FROM spatialite_history WHERE "
                 "((table_name = '%s' AND geometry_column = '%s') OR "
                 "(table_name = 'ALL-TABLES' AND geometry_column = 'ALL-GEOMETRY-COLUMNS')) AND "
                 "event = 'UpdateLayerStatistics'",
                 pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str());

    sqlite3 *hDB = poDS->GetDB();
    int nRowCount = 0, nColCount = 0;
    char **papszResult = NULL, *pszErrMsg = NULL;

    sqlite3_get_table( hDB, osSQL.c_str(), &papszResult,
                       &nRowCount, &nColCount, &pszErrMsg );

    /* Make it a Unix timestamp */
    int nYear, nMonth, nDay, nHour, nMinute, nSecond;
    struct tm brokendown;
    GIntBig nTS = -1;
    if( nRowCount >= 1 && nColCount == 1 && papszResult[1] != NULL &&
        sscanf( papszResult[1], "%04d-%02d-%02d %02d:%02d:%02d",
                &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond ) == 6 )
    {
        brokendown.tm_year = nYear - 1900;
        brokendown.tm_mon = nMonth - 1;
        brokendown.tm_mday = nDay;
        brokendown.tm_hour = nHour;
        brokendown.tm_min = nMinute;
        brokendown.tm_sec = nSecond;
        nTS = CPLYMDHMSToUnixTime(&brokendown);
    }

    /* If it is equal to the modified timestamp of the DB (as a file) */
    /* then we can safely use the data from the layer_statistics, since */
    /* it will be up-to-date */
    if( nFileTimestamp == nTS || nFileTimestamp == nTS + 1 )
    {
        osSQL.Printf("SELECT row_count, extent_min_x, extent_min_y, extent_max_x, extent_max_y "
                        "FROM layer_statistics WHERE table_name = '%s' AND geometry_column = '%s'",
                        pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str());

        sqlite3_free_table( papszResult );
        papszResult = NULL;

        sqlite3_get_table( hDB, osSQL.c_str(), &papszResult,
                            &nRowCount, &nColCount, &pszErrMsg );

        if( nRowCount == 1 )
        {
            char **papszRow = papszResult + 5;
            const char* pszRowCount = papszRow[0];
            const char* pszMinX = papszRow[1];
            const char* pszMinY = papszRow[2];
            const char* pszMaxX = papszRow[3];
            const char* pszMaxY = papszRow[4];

            CPLDebug("SQLITE", "File timestamp matches layer statistics timestamp. "
                        "Loading statistics for %s", pszTableName);

            if( pszRowCount != NULL )
            {
                nFeatureCount = (GIntBig) CPLScanUIntBig( pszRowCount, 32 );
                CPLDebug("SQLite", "Layer %s feature count : " CPL_FRMT_GIB,
                            pszTableName, nFeatureCount);
            }

            if( pszMinX != NULL && pszMinY != NULL &&
                pszMaxX != NULL && pszMaxY != NULL )
            {
                bCachedExtentIsValid = TRUE;
                oCachedExtent.MinX = CPLAtof(pszMinX);
                oCachedExtent.MinY = CPLAtof(pszMinY);
                oCachedExtent.MaxX = CPLAtof(pszMaxX);
                oCachedExtent.MaxY = CPLAtof(pszMaxY);
                CPLDebug("SQLite", "Layer %s extent : %s,%s,%s,%s",
                            pszTableName, pszMinX,pszMinY,pszMaxX,pszMaxY);
            }
        }
    }

    if( pszErrMsg )
        sqlite3_free( pszErrMsg );

    sqlite3_free_table( papszResult );
}

/************************************************************************/
/*                          SaveStatistics()                            */
/************************************************************************/

int OGRSQLiteTableLayer::SaveStatistics()
{
    if( !bStatisticsNeedsToBeFlushed || !poDS->IsSpatialiteDB()  || !poDS->IsSpatialiteLoaded() || pszGeomCol == NULL )
        return -1;

    CPLString osSQL;
    sqlite3 *hDB = poDS->GetDB();
    char* pszErrMsg = NULL;

    if( nFeatureCount >= 0 )
    {
        /* Update or add entry in the layer_statistics table */
        if( bCachedExtentIsValid )
        {
            osSQL.Printf("INSERT OR REPLACE INTO layer_statistics (raster_layer, "
                            "table_name, geometry_column, row_count, extent_min_x, "
                            "extent_min_y, extent_max_x, extent_max_y) VALUES ("
                            "0, '%s', '%s', " CPL_FRMT_GIB ", %s, %s, %s, %s)",
                            pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str(),
                            nFeatureCount,
                            // Insure that only Decimal.Points are used, never local settings such as Decimal.Comma.
                            CPLString().FormatC(oCachedExtent.MinX,"%.18g").c_str(),
                            CPLString().FormatC(oCachedExtent.MinY,"%.18g").c_str(),
                            CPLString().FormatC(oCachedExtent.MaxX,"%.18g").c_str(),
                            CPLString().FormatC(oCachedExtent.MaxY,"%.18g").c_str());
        }
        else
        {
            osSQL.Printf("INSERT OR REPLACE INTO layer_statistics (raster_layer, "
                            "table_name, geometry_column, row_count, extent_min_x, "
                            "extent_min_y, extent_max_x, extent_max_y) VALUES ("
                            "0, '%s', '%s', " CPL_FRMT_GIB ", NULL, NULL, NULL, NULL)",
                            pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str(),
                            nFeatureCount);
        }
    }
    else
    {
        /* Remove any existing entry in layer_statistics if for some reason */
        /* we know that it will out-of-sync */
        osSQL.Printf("DELETE FROM layer_statistics WHERE "
                     "table_name = '%s' AND geometry_column = '%s'",
                     pszEscapedTableName, OGRSQLiteEscape(pszGeomCol).c_str());
    }

    int rc = sqlite3_exec( hDB, osSQL.c_str(), NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLDebug("SQLITE", "Error %s", pszErrMsg ? pszErrMsg : "unknown");
        sqlite3_free( pszErrMsg );
        return FALSE;
    }
    else
        return TRUE;
}

/************************************************************************/
/*                      SetCompressedColumns()                          */
/************************************************************************/

void OGRSQLiteTableLayer::SetCompressedColumns( const char* pszCompressedColumns )
{
    papszCompressedColumns = CSLTokenizeString2( pszCompressedColumns, ",",
                                                 CSLT_HONOURSTRINGS );
}
