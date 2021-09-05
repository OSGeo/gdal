/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteViewLayer class, access to an existing spatialite view.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_sqlite.h"
#include "ogrsqliteutility.h"

#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "sqlite3.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRSQLiteViewLayer()                         */
/************************************************************************/

OGRSQLiteViewLayer::OGRSQLiteViewLayer( OGRSQLiteDataSource *poDSIn ):
    OGRSQLiteLayer(poDSIn)
{
}

/************************************************************************/
/*                        ~OGRSQLiteViewLayer()                        */
/************************************************************************/

OGRSQLiteViewLayer::~OGRSQLiteViewLayer()

{
    ClearStatement();
    CPLFree(m_pszViewName);
    CPLFree(m_pszEscapedTableName);
    CPLFree(m_pszEscapedUnderlyingTableName);
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRSQLiteViewLayer::Initialize( const char *pszViewNameIn,
                                       const char *pszViewGeometry,
                                       const char *pszViewRowid,
                                       const char *pszUnderlyingTableName,
                                       const char *pszUnderlyingGeometryColumn)

{
    m_pszViewName = CPLStrdup(pszViewNameIn);
    SetDescription( m_pszViewName );

    m_osGeomColumn = pszViewGeometry;
    m_eGeomFormat = OSGF_SpatiaLite;

    CPLFree( m_pszFIDColumn );
    m_pszFIDColumn = CPLStrdup( pszViewRowid );

    m_osUnderlyingTableName = pszUnderlyingTableName;
    m_osUnderlyingGeometryColumn = pszUnderlyingGeometryColumn;
    m_poUnderlyingLayer = nullptr;

    m_pszEscapedTableName = CPLStrdup(SQLEscapeLiteral(m_pszViewName));
    m_pszEscapedUnderlyingTableName = CPLStrdup(SQLEscapeLiteral(pszUnderlyingTableName));

    return CE_None;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn* OGRSQLiteViewLayer::GetLayerDefn()
{
    if (m_poFeatureDefn)
        return m_poFeatureDefn;

    EstablishFeatureDefn();

    if (m_poFeatureDefn == nullptr)
    {
        m_bLayerDefnError = true;

        m_poFeatureDefn = new OGRSQLiteFeatureDefn( m_pszViewName );
        m_poFeatureDefn->Reference();
    }

    return m_poFeatureDefn;
}

/************************************************************************/
/*                        GetUnderlyingLayer()                          */
/************************************************************************/

OGRSQLiteLayer* OGRSQLiteViewLayer::GetUnderlyingLayer()
{
    if( m_poUnderlyingLayer == nullptr )
    {
        if( strchr(m_osUnderlyingTableName, '(') == nullptr )
        {
            CPLString osNewUnderlyingTableName;
            osNewUnderlyingTableName.Printf("%s(%s)",
                                            m_osUnderlyingTableName.c_str(),
                                            m_osUnderlyingGeometryColumn.c_str());
            m_poUnderlyingLayer =
                (OGRSQLiteLayer*) m_poDS->GetLayerByNameNotVisible(osNewUnderlyingTableName);
        }
        if( m_poUnderlyingLayer == nullptr )
            m_poUnderlyingLayer =
                (OGRSQLiteLayer*) m_poDS->GetLayerByNameNotVisible(m_osUnderlyingTableName);
    }
    return m_poUnderlyingLayer;
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

OGRwkbGeometryType OGRSQLiteViewLayer::GetGeomType()
{
    if (m_poFeatureDefn)
        return m_poFeatureDefn->GetGeomType();

    OGRSQLiteLayer* l_m_poUnderlyingLayer = GetUnderlyingLayer();
    if (l_m_poUnderlyingLayer)
        return l_m_poUnderlyingLayer->GetGeomType();

    return wkbUnknown;
}

/************************************************************************/
/*                         EstablishFeatureDefn()                       */
/************************************************************************/

CPLErr OGRSQLiteViewLayer::EstablishFeatureDefn()
{
    sqlite3 *hDB = m_poDS->GetDB();
    sqlite3_stmt *hColStmt = nullptr;

    OGRSQLiteLayer* l_m_poUnderlyingLayer = GetUnderlyingLayer();
    if (l_m_poUnderlyingLayer == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find underlying layer %s for view %s",
                 m_osUnderlyingTableName.c_str(), m_pszViewName);
        return CE_Failure;
    }
    if ( !l_m_poUnderlyingLayer->IsTableLayer() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Underlying layer %s for view %s is not a regular table",
                 m_osUnderlyingTableName.c_str(), m_pszViewName);
        return CE_Failure;
    }

    int nUnderlyingLayerGeomFieldIndex =
        l_m_poUnderlyingLayer->GetLayerDefn()->GetGeomFieldIndex(m_osUnderlyingGeometryColumn);
    if ( nUnderlyingLayerGeomFieldIndex < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Underlying layer %s for view %s has not expected geometry column name %s",
                 m_osUnderlyingTableName.c_str(), m_pszViewName,
                 m_osUnderlyingGeometryColumn.c_str());
        return CE_Failure;
    }

    m_bHasSpatialIndex =
        l_m_poUnderlyingLayer->HasSpatialIndex(nUnderlyingLayerGeomFieldIndex);

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    hColStmt = nullptr;
    const char *pszSQL =
        CPLSPrintf( "SELECT \"%s\", * FROM '%s' LIMIT 1",
                    SQLEscapeName(m_pszFIDColumn).c_str(),
                    m_pszEscapedTableName );

    int rc = sqlite3_prepare_v2( hDB, pszSQL, -1, &hColStmt, nullptr );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to query table %s for column definitions : %s.",
                  m_pszViewName, sqlite3_errmsg(hDB) );

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
    std::set<CPLString> aosGeomCols;
    std::set<CPLString> aosIgnoredCols;
    aosGeomCols.insert(m_osGeomColumn);
    BuildFeatureDefn( m_pszViewName, false, hColStmt, &aosGeomCols, aosIgnoredCols );
    sqlite3_finalize( hColStmt );

/* -------------------------------------------------------------------- */
/*      Set the properties of the geometry column.                      */
/* -------------------------------------------------------------------- */
    if( m_poFeatureDefn->GetGeomFieldCount() != 0 )
    {
        OGRSQLiteGeomFieldDefn* poSrcGeomFieldDefn =
            l_m_poUnderlyingLayer->myGetLayerDefn()->myGetGeomFieldDefn(nUnderlyingLayerGeomFieldIndex);
        OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
            m_poFeatureDefn->myGetGeomFieldDefn(0);
        poGeomFieldDefn->SetType(poSrcGeomFieldDefn->GetType());
        poGeomFieldDefn->SetSpatialRef(poSrcGeomFieldDefn->GetSpatialRef());
        poGeomFieldDefn->m_nSRSId = poSrcGeomFieldDefn->m_nSRSId;
        if( m_eGeomFormat != OSGF_None )
            poGeomFieldDefn->m_eGeomFormat = m_eGeomFormat;
    }

    return CE_None;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRSQLiteViewLayer::ResetStatement()

{
    CPLString osSQL;

    ClearStatement();

    m_iNextShapeId = 0;

    osSQL.Printf( "SELECT \"%s\", * FROM '%s' %s",
                  SQLEscapeName(m_pszFIDColumn).c_str(),
                  m_pszEscapedTableName,
                  m_osWHERE.c_str() );

    const int rc =
        sqlite3_prepare_v2( m_poDS->GetDB(), osSQL, static_cast<int>(osSQL.size()),
                         &m_hStmt, nullptr );

    if( rc == SQLITE_OK )
    {
        return OGRERR_NONE;
    }

    CPLError( CE_Failure, CPLE_AppDefined,
              "In ResetStatement(): sqlite3_prepare_v2(%s):\n  %s",
              osSQL.c_str(), sqlite3_errmsg(m_poDS->GetDB()) );
    m_hStmt = nullptr;
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSQLiteViewLayer::GetNextFeature()

{
    if (HasLayerDefnError())
        return nullptr;

    return OGRSQLiteLayer::GetNextFeature();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRSQLiteViewLayer::GetFeature( GIntBig nFeatureId )

{
    if (HasLayerDefnError())
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we don't have an explicit FID column, just read through      */
/*      the result set iteratively to find our target.                  */
/* -------------------------------------------------------------------- */
    if( m_pszFIDColumn == nullptr )
        return OGRSQLiteLayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Setup explicit query statement to fetch the record we want.     */
/* -------------------------------------------------------------------- */
    CPLString osSQL;

    ClearStatement();

    m_iNextShapeId = nFeatureId;

    osSQL.Printf( "SELECT \"%s\", * FROM '%s' WHERE \"%s\" = %d",
                  SQLEscapeName(m_pszFIDColumn).c_str(),
                  m_pszEscapedTableName,
                  SQLEscapeName(m_pszFIDColumn).c_str(),
                  (int) nFeatureId );

    CPLDebug( "OGR_SQLITE", "exec(%s)", osSQL.c_str() );

    const int rc =
        sqlite3_prepare_v2( m_poDS->GetDB(), osSQL, static_cast<int>(osSQL.size()),
                         &m_hStmt, nullptr );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "In GetFeature(): sqlite3_prepare_v2(%s):\n  %s",
                  osSQL.c_str(), sqlite3_errmsg(m_poDS->GetDB()) );

        return nullptr;
    }
/* -------------------------------------------------------------------- */
/*      Get the feature if possible.                                    */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = GetNextRawFeature();

    ResetReading();

    return poFeature;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRSQLiteViewLayer::SetAttributeFilter( const char *pszQuery )

{
    if( pszQuery == nullptr )
        m_osQuery = "";
    else
        m_osQuery = pszQuery;

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
    if (HasLayerDefnError() || m_poFeatureDefn == nullptr ||
        iGeomCol < 0 || iGeomCol >= m_poFeatureDefn->GetGeomFieldCount())
        return "";

    if( poFilterGeom != nullptr && m_bHasSpatialIndex )
    {
        OGREnvelope  sEnvelope;

        poFilterGeom->getEnvelope( &sEnvelope );

        /* We first check that the spatial index table exists */
        if (!m_bHasCheckedSpatialIndexTable)
        {
            m_bHasCheckedSpatialIndexTable = true;
            char **papszResult = nullptr;
            int nRowCount = 0;
            int nColCount = 0;
            char *pszErrMsg = nullptr;

            CPLString osSQL;
            osSQL.Printf("SELECT name FROM sqlite_master "
                        "WHERE name='idx_%s_%s'",
                        m_pszEscapedUnderlyingTableName,
                        SQLEscapeLiteral(m_osUnderlyingGeometryColumn).c_str());

            int  rc = sqlite3_get_table( m_poDS->GetDB(), osSQL.c_str(),
                                        &papszResult, &nRowCount,
                                        &nColCount, &pszErrMsg );

            if( rc != SQLITE_OK )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Error: %s",
                        pszErrMsg );
                sqlite3_free( pszErrMsg );
                m_bHasSpatialIndex = false;
            }
            else
            {
                if (nRowCount != 1)
                {
                    m_bHasSpatialIndex = false;
                }

                sqlite3_free_table(papszResult);
            }
        }

        if (m_bHasSpatialIndex)
        {
            return FormatSpatialFilterFromRTree(poFilterGeom,
                CPLSPrintf("\"%s\"", SQLEscapeName(m_pszFIDColumn).c_str()),
                m_pszEscapedUnderlyingTableName,
                SQLEscapeLiteral(m_osUnderlyingGeometryColumn).c_str());
        }
        else
        {
            CPLDebug("SQLITE", "Count not find idx_%s_%s layer. Disabling spatial index",
                     m_pszEscapedUnderlyingTableName, m_osUnderlyingGeometryColumn.c_str());
        }
    }

    if( poFilterGeom != nullptr && m_poDS->IsSpatialiteLoaded() )
    {
        return FormatSpatialFilterFromMBR(poFilterGeom,
            SQLEscapeName(m_poFeatureDefn->GetGeomFieldDefn(iGeomCol)->GetNameRef()).c_str());
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
    m_osWHERE = "";

    CPLString osSpatialWHERE = GetSpatialWhere(m_iGeomFieldFilter,
                                               m_poFilterGeom);
    if (!osSpatialWHERE.empty())
    {
        m_osWHERE = "WHERE ";
        m_osWHERE += osSpatialWHERE;
    }

    if( !m_osQuery.empty() )
    {
        if( m_osWHERE.empty() )
        {
            m_osWHERE = "WHERE ";
            m_osWHERE += m_osQuery;
        }
        else
        {
            m_osWHERE += " AND (";
            m_osWHERE += m_osQuery;
            m_osWHERE += ")";
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
        return m_poFilterGeom == nullptr || m_osGeomColumn.empty() ||
               m_bHasSpatialIndex;

    else if (EQUAL(pszCap,OLCFastSpatialFilter))
        return m_bHasSpatialIndex;

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

GIntBig OGRSQLiteViewLayer::GetFeatureCount( int bForce )

{
    if (HasLayerDefnError())
        return 0;

    if( !TestCapability(OLCFastFeatureCount) )
        return OGRSQLiteLayer::GetFeatureCount( bForce );

/* -------------------------------------------------------------------- */
/*      Form count SQL.                                                 */
/* -------------------------------------------------------------------- */
    const char *pszSQL =
        CPLSPrintf( "SELECT count(*) FROM '%s' %s",
                    m_pszEscapedTableName, m_osWHERE.c_str() );

/* -------------------------------------------------------------------- */
/*      Execute.                                                        */
/* -------------------------------------------------------------------- */
    char **papszResult, *pszErrMsg;
    int nRowCount, nColCount;
    int nResult = -1;

    if( sqlite3_get_table( m_poDS->GetDB(), pszSQL, &papszResult,
                           &nColCount, &nRowCount, &pszErrMsg ) != SQLITE_OK )
        return -1;

    if( nRowCount == 1 && nColCount == 1 )
        nResult = atoi(papszResult[1]);

    sqlite3_free_table( papszResult );

    return nResult;
}
