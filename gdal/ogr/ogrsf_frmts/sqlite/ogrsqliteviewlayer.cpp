/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteViewLayer class, access to an existing spatialite view.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
/*                        OGRSQLiteViewLayer()                         */
/************************************************************************/

OGRSQLiteViewLayer::OGRSQLiteViewLayer( OGRSQLiteDataSource *poDSIn )

{
    poDS = poDSIn;

    iNextShapeId = 0;

    poFeatureDefn = NULL;
    pszViewName = NULL;
    pszEscapedTableName = NULL;
    pszEscapedUnderlyingTableName = NULL;

    bHasCheckedSpatialIndexTable = FALSE;

    bLayerDefnError = FALSE;
}

/************************************************************************/
/*                        ~OGRSQLiteViewLayer()                        */
/************************************************************************/

OGRSQLiteViewLayer::~OGRSQLiteViewLayer()

{
    ClearStatement();
    CPLFree(pszViewName);
    CPLFree(pszEscapedTableName);
    CPLFree(pszEscapedUnderlyingTableName);
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRSQLiteViewLayer::Initialize( const char *pszViewName,
                                       const char *pszViewGeometry,
                                       const char *pszViewRowid,
                                       const char *pszUnderlyingTableName,
                                       const char *pszUnderlyingGeometryColumn)

{
    this->pszViewName = CPLStrdup(pszViewName);
    SetDescription( pszViewName );

    osGeomColumn = pszViewGeometry;
    eGeomFormat = OSGF_SpatiaLite;

    CPLFree( pszFIDColumn );
    pszFIDColumn = CPLStrdup( pszViewRowid );

    osUnderlyingTableName = pszUnderlyingTableName;
    osUnderlyingGeometryColumn = pszUnderlyingGeometryColumn;
    poUnderlyingLayer = NULL;

    //this->bHasM = bHasM;

    pszEscapedTableName = CPLStrdup(OGRSQLiteEscape(pszViewName));
    pszEscapedUnderlyingTableName = CPLStrdup(OGRSQLiteEscape(pszUnderlyingTableName));

    return CE_None;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn* OGRSQLiteViewLayer::GetLayerDefn()
{
    if (poFeatureDefn)
        return poFeatureDefn;

    EstablishFeatureDefn();

    if (poFeatureDefn == NULL)
    {
        bLayerDefnError = TRUE;

        poFeatureDefn = new OGRSQLiteFeatureDefn( pszViewName );
        poFeatureDefn->Reference();
    }

    return poFeatureDefn;
}

/************************************************************************/
/*                        GetUnderlyingLayer()                          */
/************************************************************************/

OGRSQLiteLayer* OGRSQLiteViewLayer::GetUnderlyingLayer()
{
    if( poUnderlyingLayer == NULL )
    {
        if( strchr(osUnderlyingTableName, '(') == NULL )
        {
            CPLString osNewUnderlyingTableName;
            osNewUnderlyingTableName.Printf("%s(%s)",
                                            osUnderlyingTableName.c_str(),
                                            osUnderlyingGeometryColumn.c_str());
            poUnderlyingLayer =
                (OGRSQLiteLayer*) poDS->GetLayerByName(osNewUnderlyingTableName);
        }
        if( poUnderlyingLayer == NULL )
            poUnderlyingLayer =
                (OGRSQLiteLayer*) poDS->GetLayerByName(osUnderlyingTableName);
    }
    return poUnderlyingLayer;
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

OGRwkbGeometryType OGRSQLiteViewLayer::GetGeomType()
{
    if (poFeatureDefn)
        return poFeatureDefn->GetGeomType();

    OGRSQLiteLayer* poUnderlyingLayer = GetUnderlyingLayer();
    if (poUnderlyingLayer)
        return poUnderlyingLayer->GetGeomType();

    return wkbUnknown;
}

/************************************************************************/
/*                         EstablishFeatureDefn()                       */
/************************************************************************/

CPLErr OGRSQLiteViewLayer::EstablishFeatureDefn()
{
    int rc;
    sqlite3 *hDB = poDS->GetDB();
    sqlite3_stmt *hColStmt = NULL;
    const char *pszSQL;

    OGRSQLiteLayer* poUnderlyingLayer = GetUnderlyingLayer();
    if (poUnderlyingLayer == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find underlying layer %s for view %s",
                 osUnderlyingTableName.c_str(), pszViewName);
        return CE_Failure;
    }
    if ( !poUnderlyingLayer->IsTableLayer() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Underlying layer %s for view %s is not a regular table",
                 osUnderlyingTableName.c_str(), pszViewName);
        return CE_Failure;
    }

    const char* pszRealUnderlyingGeometryColumn = poUnderlyingLayer->GetGeometryColumn();
    if ( pszRealUnderlyingGeometryColumn == NULL ||
         !EQUAL(pszRealUnderlyingGeometryColumn, osUnderlyingGeometryColumn.c_str()) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Underlying layer %s for view %s has not expected geometry column name (%s instead of %s)",
                 osUnderlyingTableName.c_str(), pszViewName,
                 pszRealUnderlyingGeometryColumn ? pszRealUnderlyingGeometryColumn : "(null)",
                 osUnderlyingGeometryColumn.c_str());
        return CE_Failure;
    }

    this->bHasSpatialIndex = poUnderlyingLayer->HasSpatialIndex();

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    hColStmt = NULL;
    pszSQL = CPLSPrintf( "SELECT \"%s\", * FROM '%s' LIMIT 1",
                         OGRSQLiteEscapeName(pszFIDColumn).c_str(),
                         pszEscapedTableName );

    rc = sqlite3_prepare( hDB, pszSQL, strlen(pszSQL), &hColStmt, NULL ); 
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to query table %s for column definitions : %s.",
                  pszViewName, sqlite3_errmsg(hDB) );
        
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
/*      Collect the rest of the fields.                                 */
/* -------------------------------------------------------------------- */
    std::set<CPLString> aosEmpty;
    BuildFeatureDefn( pszViewName, hColStmt, osGeomColumn, aosEmpty );
    sqlite3_finalize( hColStmt );

/* -------------------------------------------------------------------- */
/*      Set the properties of the geometry column.                      */
/* -------------------------------------------------------------------- */
    if( poFeatureDefn->GetGeomFieldCount() != 0 )
    {
        poFeatureDefn->SetGeomType( poUnderlyingLayer->GetGeomType() );
        OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->myGetGeomFieldDefn(0);
        poGeomFieldDefn->SetSpatialRef(poUnderlyingLayer->GetSpatialRef());
        poGeomFieldDefn->nSRSId = poUnderlyingLayer->myGetLayerDefn()->
            myGetGeomFieldDefn(0)->nSRSId;
        if( eGeomFormat != OSGF_None )
            poGeomFieldDefn->eGeomFormat = eGeomFormat;
    }

    return CE_None;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRSQLiteViewLayer::ResetStatement()

{
    int rc;
    CPLString osSQL;

    ClearStatement();

    iNextShapeId = 0;

    osSQL.Printf( "SELECT \"%s\", * FROM '%s' %s",
                  OGRSQLiteEscapeName(pszFIDColumn).c_str(),
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
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSQLiteViewLayer::GetNextFeature()

{
    if (HasLayerDefnError())
        return NULL;

    return OGRSQLiteLayer::GetNextFeature();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRSQLiteViewLayer::GetFeature( long nFeatureId )

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

    osSQL.Printf( "SELECT \"%s\", * FROM '%s' WHERE \"%s\" = %d",
                  OGRSQLiteEscapeName(pszFIDColumn).c_str(),
                  pszEscapedTableName, 
                  OGRSQLiteEscapeName(pszFIDColumn).c_str(),
                  (int) nFeatureId );

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

OGRErr OGRSQLiteViewLayer::SetAttributeFilter( const char *pszQuery )

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

void OGRSQLiteViewLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                           GetSpatialWhere()                          */
/************************************************************************/

CPLString OGRSQLiteViewLayer::GetSpatialWhere(int iGeomCol,
                                              OGRGeometry* poFilterGeom)
{
    if (HasLayerDefnError() || poFeatureDefn == NULL ||
        iGeomCol < 0 || iGeomCol >= poFeatureDefn->GetGeomFieldCount())
        return "";

    if( poFilterGeom != NULL && bHasSpatialIndex )
    {
        OGREnvelope  sEnvelope;

        poFilterGeom->getEnvelope( &sEnvelope );

        /* We first check that the spatial index table exists */
        if (!bHasCheckedSpatialIndexTable)
        {
            bHasCheckedSpatialIndexTable = TRUE;
            char **papszResult;
            int nRowCount, nColCount;
            char *pszErrMsg = NULL;

            CPLString osSQL;
            osSQL.Printf("SELECT name FROM sqlite_master "
                        "WHERE name='idx_%s_%s'",
                        pszEscapedUnderlyingTableName,
                        OGRSQLiteEscape(osUnderlyingGeometryColumn).c_str());

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
                }

                sqlite3_free_table(papszResult);
            }
        }

        if (bHasSpatialIndex)
        {
            return FormatSpatialFilterFromRTree(poFilterGeom,
                CPLSPrintf("\"%s\"", OGRSQLiteEscapeName(pszFIDColumn).c_str()),
                pszEscapedUnderlyingTableName,
                OGRSQLiteEscape(osUnderlyingGeometryColumn).c_str());
        }
        else
        {
            CPLDebug("SQLITE", "Count not find idx_%s_%s layer. Disabling spatial index",
                     pszEscapedUnderlyingTableName, osUnderlyingGeometryColumn.c_str());
        }

    }

    if( poFilterGeom != NULL && poDS->IsSpatialiteLoaded() )
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

void OGRSQLiteViewLayer::BuildWhere()

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

int OGRSQLiteViewLayer::TestCapability( const char * pszCap )

{
    if (HasLayerDefnError())
        return FALSE;

    if (EQUAL(pszCap,OLCFastFeatureCount))
        return m_poFilterGeom == NULL || osGeomColumn.size() == 0 ||
               bHasSpatialIndex;

    else if (EQUAL(pszCap,OLCFastSpatialFilter))
        return bHasSpatialIndex;

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

int OGRSQLiteViewLayer::GetFeatureCount( int bForce )

{
    if (HasLayerDefnError())
        return 0;

    if( !TestCapability(OLCFastFeatureCount) )
        return OGRSQLiteLayer::GetFeatureCount( bForce );

/* -------------------------------------------------------------------- */
/*      Form count SQL.                                                 */
/* -------------------------------------------------------------------- */
    const char *pszSQL;

    pszSQL = CPLSPrintf( "SELECT count(*) FROM '%s' %s",
                          pszEscapedTableName, osWHERE.c_str() );

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
