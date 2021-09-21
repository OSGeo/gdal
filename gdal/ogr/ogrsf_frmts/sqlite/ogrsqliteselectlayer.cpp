/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteSelectLayer class, layer access to the results
 *           of a SELECT statement executed via ExecuteSQL().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "sqlite3.h"
#include "ogr_swq.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                   OGRSQLiteSelectLayerCommonBehaviour()              */
/************************************************************************/

OGRSQLiteSelectLayerCommonBehaviour::OGRSQLiteSelectLayerCommonBehaviour(
    OGRSQLiteBaseDataSource* poDSIn,
    IOGRSQLiteSelectLayer* m_poLayerIn,
    const CPLString& osSQLIn,
    bool bEmptyLayerIn) :
    m_poDS(poDSIn),
    m_poLayer(m_poLayerIn),
    m_osSQLBase(osSQLIn),
    m_bEmptyLayer(bEmptyLayerIn),
    m_osSQLCurrent(osSQLIn)
{}

/************************************************************************/
/*                        OGRSQLiteSelectLayer()                        */
/************************************************************************/

OGRSQLiteSelectLayer::OGRSQLiteSelectLayer( OGRSQLiteDataSource *poDSIn,
                                            const CPLString& osSQLIn,
                                            sqlite3_stmt *m_hStmtIn,
                                            bool bUseStatementForGetNextFeature,
                                            bool bEmptyLayer,
                                            bool bAllowMultipleGeomFieldsIn ):
    OGRSQLiteLayer(poDSIn),
    m_poBehavior(new OGRSQLiteSelectLayerCommonBehaviour(poDSIn, this, osSQLIn,
                                                        bEmptyLayer))
{
    m_bAllowMultipleGeomFields = bAllowMultipleGeomFieldsIn;

    std::set<CPLString> aosEmpty;
    BuildFeatureDefn( "SELECT", true, m_hStmtIn, nullptr, aosEmpty );
    SetDescription( "SELECT" );

    if( bUseStatementForGetNextFeature )
    {
        m_hStmt = m_hStmtIn;
        m_bDoStep = false;

        // Try to extract SRS from first geometry
        for( int iField = 0;
             !bEmptyLayer && iField < m_poFeatureDefn->GetGeomFieldCount();
             iField ++)
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                m_poFeatureDefn->myGetGeomFieldDefn(iField);
            if( wkbFlatten(poGeomFieldDefn->GetType()) != wkbUnknown )
                continue;

            if( sqlite3_column_type( m_hStmt, poGeomFieldDefn->m_iCol ) == SQLITE_BLOB &&
                sqlite3_column_bytes( m_hStmt, poGeomFieldDefn->m_iCol ) > 39 )
            {
                const GByte* pabyBlob = (const GByte*)sqlite3_column_blob( m_hStmt, poGeomFieldDefn->m_iCol );
                int eByteOrder = pabyBlob[1];
                if( pabyBlob[0] == 0x00 &&
                    (eByteOrder == wkbNDR || eByteOrder == wkbXDR) &&
                    pabyBlob[38] == 0x7C )
                {
                    int nSRSId = 0;
                    memcpy(&nSRSId, pabyBlob + 2, 4);
#ifdef CPL_LSB
                    if( eByteOrder != wkbNDR)
                        CPL_SWAP32PTR(&nSRSId);
#else
                    if( eByteOrder == wkbNDR)
                        CPL_SWAP32PTR(&nSRSId);
#endif
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    OGRSpatialReference* poSRS = m_poDS->FetchSRS( nSRSId );
                    CPLPopErrorHandler();
                    if( poSRS != nullptr )
                    {
                        poGeomFieldDefn->m_nSRSId = nSRSId;
                        poGeomFieldDefn->SetSpatialRef(poSRS);
                    }
                    else
                        CPLErrorReset();
                }
#ifdef SQLITE_HAS_COLUMN_METADATA
                else if( iField == 0 )
                {
                    const char* pszTableName = sqlite3_column_table_name( m_hStmt, poGeomFieldDefn->m_iCol );
                    if( pszTableName != nullptr )
                    {
                        OGRSQLiteLayer* m_poLayer = (OGRSQLiteLayer*)
                                        m_poDS->GetLayerByName(pszTableName);
                        if( m_poLayer != nullptr &&  m_poLayer->GetLayerDefn()->GetGeomFieldCount() > 0)
                        {
                            OGRSQLiteGeomFieldDefn* poSrcGFldDefn =
                                m_poLayer->myGetLayerDefn()->myGetGeomFieldDefn(0);
                            poGeomFieldDefn->m_nSRSId = poSrcGFldDefn->m_nSRSId;
                            poGeomFieldDefn->SetSpatialRef(poSrcGFldDefn->GetSpatialRef());
                        }
                    }
                }
#endif
            }
        }
    }
    else
        sqlite3_finalize( m_hStmtIn );
}

/************************************************************************/
/*                       ~OGRSQLiteSelectLayer()                        */
/************************************************************************/

OGRSQLiteSelectLayer::~OGRSQLiteSelectLayer()
{
    delete m_poBehavior;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSQLiteSelectLayer::ResetReading()
{
    return m_poBehavior->ResetReading();
}

void OGRSQLiteSelectLayerCommonBehaviour::ResetReading()
{
    if( m_poLayer->HasReadFeature() || m_bAllowResetReadingEvenIfIndexAtZero )
    {
        m_poLayer->BaseResetReading();
        m_bAllowResetReadingEvenIfIndexAtZero = false;
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSQLiteSelectLayer::GetNextFeature()
{
    return m_poBehavior->GetNextFeature();
}

OGRFeature *OGRSQLiteSelectLayerCommonBehaviour::GetNextFeature()
{
    if( m_bEmptyLayer )
        return nullptr;

    return m_poLayer->BaseGetNextFeature();
}

/************************************************************************/
/*               OGRGenSQLResultsLayerHasSpecialField()                 */
/************************************************************************/

static
int HasSpecialFields(swq_expr_node* expr, int nMinIndexForSpecialField)
{
    if (expr->eNodeType == SNT_COLUMN)
    {
        if (expr->table_index == 0)
        {
            return expr->field_index >= nMinIndexForSpecialField &&
                   expr->field_index < nMinIndexForSpecialField + SPECIAL_FIELD_COUNT;
        }
    }
    else if (expr->eNodeType == SNT_OPERATION)
    {
        for( int i = 0; i < expr->nSubExprCount; i++ )
        {
            if (HasSpecialFields(expr->papoSubExpr[i], nMinIndexForSpecialField))
                return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRSQLiteSelectLayer::SetAttributeFilter( const char *pszQuery )
{
    return m_poBehavior->SetAttributeFilter(pszQuery);
}

OGRErr OGRSQLiteSelectLayerCommonBehaviour::SetAttributeFilter( const char *pszQuery )

{
    char*& m_pszAttrQuertyString = m_poLayer->GetAttrQueryString();
    if( m_pszAttrQuertyString == nullptr && pszQuery == nullptr )
        return OGRERR_NONE;

    CPLFree(m_pszAttrQuertyString);
    m_pszAttrQuertyString = (pszQuery) ? CPLStrdup(pszQuery) : nullptr;

    m_bAllowResetReadingEvenIfIndexAtZero = true;

    OGRFeatureQuery oQuery;

    CPLPushErrorHandler(CPLQuietErrorHandler);
    const bool bHasSpecialFields = (pszQuery != nullptr && pszQuery[0] != '\0' &&
        oQuery.Compile( m_poLayer->GetLayerDefn(), pszQuery ) == OGRERR_NONE &&
        HasSpecialFields((swq_expr_node*)oQuery.GetSWQExpr(), m_poLayer->GetLayerDefn()->GetFieldCount()) );
    CPLPopErrorHandler();

    if( bHasSpecialFields || !BuildSQL() )
    {
        return m_poLayer->BaseSetAttributeFilter(pszQuery);
    }

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

GIntBig OGRSQLiteSelectLayer::GetFeatureCount( int bForce )
{
    return m_poBehavior->GetFeatureCount(bForce);
}

GIntBig OGRSQLiteSelectLayerCommonBehaviour::GetFeatureCount( int bForce )
{
    if( m_bEmptyLayer )
        return 0;

    if( m_poLayer->GetFeatureQuery() == nullptr &&
        STARTS_WITH_CI(m_osSQLCurrent, "SELECT COUNT(*) FROM") &&
        m_osSQLCurrent.ifind(" GROUP BY ") == std::string::npos &&
        m_osSQLCurrent.ifind(" UNION ") == std::string::npos &&
        m_osSQLCurrent.ifind(" INTERSECT ") == std::string::npos &&
        m_osSQLCurrent.ifind(" EXCEPT ") == std::string::npos )
        return 1;

    if( m_poLayer->GetFeatureQuery() != nullptr || (m_poLayer->GetFilterGeom() != nullptr && !m_bSpatialFilterInSQL) )
        return m_poLayer->BaseGetFeatureCount(bForce);

    CPLString osFeatureCountSQL("SELECT COUNT(*) FROM (");
    osFeatureCountSQL += m_osSQLCurrent;
    osFeatureCountSQL += ")";

    CPLDebug("SQLITE", "Running %s", osFeatureCountSQL.c_str());

/* -------------------------------------------------------------------- */
/*      Execute.                                                        */
/* -------------------------------------------------------------------- */
    char *pszErrMsg = nullptr;
    char **papszResult = nullptr;
    int nRowCount = 0;
    int nColCount = 0;
    int nResult = -1;

    if( sqlite3_get_table( m_poDS->GetDB(), osFeatureCountSQL, &papszResult,
                           &nRowCount, &nColCount, &pszErrMsg ) != SQLITE_OK )
    {
        CPLDebug("SQLITE", "Error: %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return m_poLayer->BaseGetFeatureCount(bForce);
    }

    if( nRowCount == 1 && nColCount == 1 )
    {
        nResult = atoi(papszResult[1]);
    }

    sqlite3_free_table( papszResult );

    return nResult;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRSQLiteSelectLayer::ResetStatement()

{
    ClearStatement();

    m_iNextShapeId = 0;
    m_bDoStep = true;

#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "prepare_v2(%s)", m_poBehavior->m_osSQLCurrent.c_str() );
#endif

    const int rc =
        sqlite3_prepare_v2( m_poDS->GetDB(), m_poBehavior->m_osSQLCurrent,
                         static_cast<int>(m_poBehavior->m_osSQLCurrent.size()),
                         &m_hStmt, nullptr );

    if( rc == SQLITE_OK )
        return OGRERR_NONE;

    CPLError( CE_Failure, CPLE_AppDefined,
              "In ResetStatement(): sqlite3_prepare_v2(%s):\n  %s",
              m_poBehavior->m_osSQLCurrent.c_str(), sqlite3_errmsg(m_poDS->GetDB()) );
    m_hStmt = nullptr;
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRSQLiteSelectLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    m_poBehavior->SetSpatialFilter(iGeomField, poGeomIn);
}

void OGRSQLiteSelectLayerCommonBehaviour::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField == 0 && poGeomIn == nullptr && m_poLayer->GetLayerDefn()->GetGeomFieldCount() == 0 )
    {
        /* do nothing */
    }
    else if( iGeomField < 0 || iGeomField >= m_poLayer->GetLayerDefn()->GetGeomFieldCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Invalid geometry field index : %d", iGeomField);
        return;
    }

    m_bAllowResetReadingEvenIfIndexAtZero = true;

    int& iGeomFieldFilter = m_poLayer->GetIGeomFieldFilter();
    iGeomFieldFilter = iGeomField;
    if( m_poLayer->InstallFilter( poGeomIn ) )
    {
        BuildSQL();

        ResetReading();
    }
}

/************************************************************************/
/*                            GetBaseLayer()                            */
/************************************************************************/

std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> OGRSQLiteSelectLayerCommonBehaviour::GetBaseLayer(size_t& i)
{
    char** papszTokens = CSLTokenizeString(m_osSQLBase.c_str());
    bool bCanInsertFilter = true;
    int nCountSelect = 0, nCountFrom = 0, nCountWhere = 0;

    for(int iToken = 0; papszTokens[iToken] != nullptr; iToken++)
    {
        if (EQUAL(papszTokens[iToken], "SELECT"))
            nCountSelect ++;
        else if (EQUAL(papszTokens[iToken], "FROM"))
            nCountFrom ++;
        else if (EQUAL(papszTokens[iToken], "WHERE"))
            nCountWhere ++;
        else if (EQUAL(papszTokens[iToken], "UNION") ||
                 EQUAL(papszTokens[iToken], "JOIN") ||
                 EQUAL(papszTokens[iToken], "INTERSECT") ||
                 EQUAL(papszTokens[iToken], "EXCEPT"))
        {
            bCanInsertFilter = false;
        }
    }
    CSLDestroy(papszTokens);

    if (!(bCanInsertFilter && nCountSelect == 1 && nCountFrom == 1 && nCountWhere <= 1))
    {
        CPLDebug("SQLITE", "SQL expression too complex to analyse");
        return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>((OGRLayer*)nullptr, (IOGRSQLiteGetSpatialWhere*)nullptr);
    }

    size_t nFromPos = m_osSQLBase.ifind(" from ");
    if (nFromPos == std::string::npos)
    {
        return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>((OGRLayer*)nullptr, (IOGRSQLiteGetSpatialWhere*)nullptr);
    }

    /* Remove potential quotes around layer name */
    char chFirst = m_osSQLBase[nFromPos + 6];
    bool bInQuotes = (chFirst == '\'' || chFirst == '"' );
    CPLString osBaseLayerName;
    for( i = nFromPos + 6 + (bInQuotes ? 1 : 0);
         i < m_osSQLBase.size(); i++ )
    {
        if (m_osSQLBase[i] == chFirst && bInQuotes )
        {
            if( i + 1 < m_osSQLBase.size() &&
                m_osSQLBase[i + 1] == chFirst )
            {
                osBaseLayerName += m_osSQLBase[i];
                i++;
            }
            else
            {
                i++;
                break;
            }
        }
        else if (m_osSQLBase[i] == ' ' && !bInQuotes)
            break;
        else
            osBaseLayerName += m_osSQLBase[i];
    }

    std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> oPair;
    if( strchr(osBaseLayerName, '(') == nullptr &&
        m_poLayer->GetLayerDefn()->GetGeomFieldCount() != 0 )
    {
        CPLString osNewUnderlyingTableName;
        osNewUnderlyingTableName.Printf("%s(%s)",
                                        osBaseLayerName.c_str(),
                                        m_poLayer->GetLayerDefn()->GetGeomFieldDefn(0)->GetNameRef());
        oPair = m_poDS->GetLayerWithGetSpatialWhereByName(osNewUnderlyingTableName);
    }
    if( oPair.first == nullptr )
        oPair = m_poDS->GetLayerWithGetSpatialWhereByName(osBaseLayerName);

    if( oPair.first != nullptr && m_poLayer->GetSpatialRef() != nullptr &&
        oPair.first->GetSpatialRef() != nullptr &&
        m_poLayer->GetSpatialRef() != oPair.first->GetSpatialRef() &&
        !m_poLayer->GetSpatialRef()->IsSame(oPair.first->GetSpatialRef()) )
    {
        CPLDebug("SQLITE", "Result layer and base layer don't have the same SRS.");
        return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>((OGRLayer*)nullptr, (IOGRSQLiteGetSpatialWhere*)nullptr);
    }

    return oPair;
}

/************************************************************************/
/*                             BuildSQL()                               */
/************************************************************************/

int OGRSQLiteSelectLayerCommonBehaviour::BuildSQL()

{
    m_osSQLCurrent = m_osSQLBase;
    m_bSpatialFilterInSQL = true;

    size_t i = 0;
    std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> oPair = GetBaseLayer(i);
    OGRLayer* poBaseLayer = oPair.first;
    if (poBaseLayer == nullptr)
    {
        CPLDebug("SQLITE", "Cannot find base layer");
        m_bSpatialFilterInSQL = false;
        return FALSE;
    }

    CPLString osSpatialWhere;
    if (m_poLayer->GetFilterGeom() != nullptr)
    {
        const char* pszGeomCol =
            m_poLayer->GetLayerDefn()->GetGeomFieldDefn(m_poLayer->GetIGeomFieldFilter())->GetNameRef();
        int nIdx = poBaseLayer->GetLayerDefn()->GetGeomFieldIndex(pszGeomCol);
        if( nIdx < 0 )
        {
            CPLDebug("SQLITE", "Cannot find field %s in base layer", pszGeomCol);
            m_bSpatialFilterInSQL = false;
        }
        else
        {
            osSpatialWhere = oPair.second->GetSpatialWhere(nIdx, m_poLayer->GetFilterGeom());
            if (osSpatialWhere.empty())
            {
                CPLDebug("SQLITE", "Cannot get spatial where clause");
                m_bSpatialFilterInSQL = false;
            }
        }
    }

    CPLString osCustomWhere;
    if( !osSpatialWhere.empty() )
    {
        osCustomWhere = osSpatialWhere;
    }
    if( m_poLayer->GetAttrQueryString() != nullptr && m_poLayer->GetAttrQueryString()[0] != '\0' )
    {
        if( !osSpatialWhere.empty())
            osCustomWhere += " AND (";
        osCustomWhere += m_poLayer->GetAttrQueryString();
        if( !osSpatialWhere.empty())
            osCustomWhere += ")";
    }

    /* Nothing to do */
    if( osCustomWhere.empty() )
        return TRUE;

    while (i < m_osSQLBase.size() && m_osSQLBase[i] == ' ')
        i ++;

    if (i < m_osSQLBase.size() && STARTS_WITH_CI(m_osSQLBase.c_str() + i, "WHERE "))
    {
        m_osSQLCurrent = m_osSQLBase.substr(0, i + 6);
        m_osSQLCurrent += osCustomWhere;
        m_osSQLCurrent += " AND (";

        size_t nEndOfWhere = m_osSQLBase.ifind(" GROUP ");
        if (nEndOfWhere == std::string::npos)
            nEndOfWhere = m_osSQLBase.ifind(" ORDER ");
        if (nEndOfWhere == std::string::npos)
            nEndOfWhere = m_osSQLBase.ifind(" LIMIT ");

        if (nEndOfWhere == std::string::npos)
        {
            m_osSQLCurrent += m_osSQLBase.substr(i + 6);
            m_osSQLCurrent += ")";
        }
        else
        {
            m_osSQLCurrent += m_osSQLBase.substr(i + 6, nEndOfWhere - (i + 6));
            m_osSQLCurrent += ")";
            m_osSQLCurrent += m_osSQLBase.substr(nEndOfWhere);
        }
    }
    else if (i < m_osSQLBase.size() &&
             (STARTS_WITH_CI(m_osSQLBase.c_str() + i, "GROUP ") ||
              STARTS_WITH_CI(m_osSQLBase.c_str() + i, "ORDER ") ||
              STARTS_WITH_CI(m_osSQLBase.c_str() + i, "LIMIT ")))
    {
        m_osSQLCurrent = m_osSQLBase.substr(0, i);
        m_osSQLCurrent += " WHERE ";
        m_osSQLCurrent += osCustomWhere;
        m_osSQLCurrent += " ";
        m_osSQLCurrent += m_osSQLBase.substr(i);
    }
    else if (i == m_osSQLBase.size())
    {
        m_osSQLCurrent = m_osSQLBase.substr(0, i);
        m_osSQLCurrent += " WHERE ";
        m_osSQLCurrent += osCustomWhere;
    }
    else
    {
        CPLDebug("SQLITE", "SQL expression too complex for the driver to insert attribute and/or spatial filter in it");
        m_bSpatialFilterInSQL = false;
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteSelectLayer::TestCapability( const char * pszCap )
{
    return m_poBehavior->TestCapability(pszCap);
}

int OGRSQLiteSelectLayerCommonBehaviour::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap,OLCFastSpatialFilter))
    {
        size_t i = 0;
        std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> oPair = GetBaseLayer(i);
        if (oPair.first == nullptr)
        {
            CPLDebug("SQLITE", "Cannot find base layer");
            return FALSE;
        }

        return oPair.second->HasFastSpatialFilter(0);
    }
    else
        return m_poLayer->BaseTestCapability( pszCap );
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRSQLiteSelectLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    return m_poBehavior->GetExtent(iGeomField, psExtent, bForce);
}

OGRErr OGRSQLiteSelectLayerCommonBehaviour::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    if( iGeomField < 0 || iGeomField >= m_poLayer->GetLayerDefn()->GetGeomFieldCount() ||
        m_poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    /* Caching of extent by SQL string is interesting to speed-up the */
    /* establishment of the WFS GetCapabilities document for a MapServer mapfile */
    /* which has several layers, only differing by scale rules */
    if( iGeomField == 0 )
    {
        const OGREnvelope* psCachedExtent = m_poDS->GetEnvelopeFromSQL(m_osSQLBase);
        if (psCachedExtent)
        {
            *psExtent = *psCachedExtent;
            return OGRERR_NONE;
        }
    }

    CPLString osSQLCommand = m_osSQLBase;

    /* ORDER BY are costly to evaluate and are not necessary to establish */
    /* the layer extent. */
    size_t nOrderByPos = osSQLCommand.ifind(" ORDER BY ");
    if( osSQLCommand.ifind("SELECT ") == 0 &&
        osSQLCommand.ifind("SELECT ", 1) == std::string::npos && /* Ensure there's no sub SELECT that could confuse our heuristics */
        nOrderByPos != std::string::npos &&
        osSQLCommand.ifind(" LIMIT ") == std::string::npos &&
        osSQLCommand.ifind(" UNION ") == std::string::npos &&
        osSQLCommand.ifind(" INTERSECT ") == std::string::npos &&
        osSQLCommand.ifind(" EXCEPT ") == std::string::npos)
    {
        osSQLCommand.resize(nOrderByPos);

        OGRLayer* poTmpLayer = m_poDS->ExecuteSQL(osSQLCommand.c_str(), nullptr, nullptr);
        if (poTmpLayer)
        {
            OGRErr eErr = poTmpLayer->GetExtent(iGeomField, psExtent, bForce);
            m_poDS->ReleaseResultSet(poTmpLayer);
            return eErr;
        }
    }

    OGRErr eErr;
    if( iGeomField == 0 )
        eErr = m_poLayer->BaseGetExtent(psExtent, bForce);
    else
        eErr = m_poLayer->BaseGetExtent(iGeomField, psExtent, bForce);
    if (iGeomField == 0 && eErr == OGRERR_NONE && m_poDS->GetUpdate() == false)
        m_poDS->SetEnvelopeForSQL(m_osSQLBase, *psExtent);
    return eErr;
}
