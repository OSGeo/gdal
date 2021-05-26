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
    IOGRSQLiteSelectLayer* poLayerIn,
    CPLString osSQLIn,
    int bEmptyLayerIn) :
    poDS(poDSIn),
    poLayer(poLayerIn),
    osSQLBase(osSQLIn),
    bEmptyLayer(bEmptyLayerIn),
    bAllowResetReadingEvenIfIndexAtZero(FALSE),
    bSpatialFilterInSQL(TRUE),
    osSQLCurrent(osSQLIn)
{}

/************************************************************************/
/*                        OGRSQLiteSelectLayer()                        */
/************************************************************************/

OGRSQLiteSelectLayer::OGRSQLiteSelectLayer( OGRSQLiteDataSource *poDSIn,
                                            CPLString osSQLIn,
                                            sqlite3_stmt *hStmtIn,
                                            int bUseStatementForGetNextFeature,
                                            int bEmptyLayer,
                                            int bAllowMultipleGeomFieldsIn )
{
    poDS = poDSIn;
    // Cannot be moved to initializer list because of use of this, which MSVC 2008 doesn't like
    poBehavior = new OGRSQLiteSelectLayerCommonBehaviour(poDSIn, this, osSQLIn,
                                                          bEmptyLayer);

    bAllowMultipleGeomFields = bAllowMultipleGeomFieldsIn;

    std::set<CPLString> aosEmpty;
    BuildFeatureDefn( "SELECT", true, hStmtIn, nullptr, aosEmpty );
    SetDescription( "SELECT" );

    if( bUseStatementForGetNextFeature )
    {
        hStmt = hStmtIn;
        bDoStep = FALSE;

        // Try to extract SRS from first geometry
        for( int iField = 0;
             !bEmptyLayer && iField < poFeatureDefn->GetGeomFieldCount();
             iField ++)
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                poFeatureDefn->myGetGeomFieldDefn(iField);
            if( wkbFlatten(poGeomFieldDefn->GetType()) != wkbUnknown )
                continue;

            if( sqlite3_column_type( hStmt, poGeomFieldDefn->iCol ) == SQLITE_BLOB &&
                sqlite3_column_bytes( hStmt, poGeomFieldDefn->iCol ) > 39 )
            {
                const GByte* pabyBlob = (const GByte*)sqlite3_column_blob( hStmt, poGeomFieldDefn->iCol );
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
                    OGRSpatialReference* poSRS = poDS->FetchSRS( nSRSId );
                    CPLPopErrorHandler();
                    if( poSRS != nullptr )
                    {
                        poGeomFieldDefn->nSRSId = nSRSId;
                        poGeomFieldDefn->SetSpatialRef(poSRS);
                    }
                    else
                        CPLErrorReset();
                }
#ifdef SQLITE_HAS_COLUMN_METADATA
                else if( iField == 0 )
                {
                    const char* pszTableName = sqlite3_column_table_name( hStmt, poGeomFieldDefn->iCol );
                    if( pszTableName != nullptr )
                    {
                        OGRSQLiteLayer* poLayer = (OGRSQLiteLayer*)
                                        poDS->GetLayerByName(pszTableName);
                        if( poLayer != nullptr &&  poLayer->GetLayerDefn()->GetGeomFieldCount() > 0)
                        {
                            OGRSQLiteGeomFieldDefn* poSrcGFldDefn =
                                poLayer->myGetLayerDefn()->myGetGeomFieldDefn(0);
                            poGeomFieldDefn->nSRSId = poSrcGFldDefn->nSRSId;
                            poGeomFieldDefn->SetSpatialRef(poSrcGFldDefn->GetSpatialRef());
                        }
                    }
                }
#endif
            }
        }
    }
    else
        sqlite3_finalize( hStmtIn );
}

/************************************************************************/
/*                       ~OGRSQLiteSelectLayer()                        */
/************************************************************************/

OGRSQLiteSelectLayer::~OGRSQLiteSelectLayer()
{
    delete poBehavior;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSQLiteSelectLayer::ResetReading()
{
    return poBehavior->ResetReading();
}

void OGRSQLiteSelectLayerCommonBehaviour::ResetReading()
{
    if( poLayer->HasReadFeature() || bAllowResetReadingEvenIfIndexAtZero )
    {
        poLayer->BaseResetReading();
        bAllowResetReadingEvenIfIndexAtZero = FALSE;
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSQLiteSelectLayer::GetNextFeature()
{
    return poBehavior->GetNextFeature();
}

OGRFeature *OGRSQLiteSelectLayerCommonBehaviour::GetNextFeature()
{
    if( bEmptyLayer )
        return nullptr;

    return poLayer->BaseGetNextFeature();
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
    return poBehavior->SetAttributeFilter(pszQuery);
}

OGRErr OGRSQLiteSelectLayerCommonBehaviour::SetAttributeFilter( const char *pszQuery )

{
    char*& m_pszAttrQuertyString = poLayer->GetAttrQueryString();
    if( m_pszAttrQuertyString == nullptr && pszQuery == nullptr )
        return OGRERR_NONE;

    CPLFree(m_pszAttrQuertyString);
    m_pszAttrQuertyString = (pszQuery) ? CPLStrdup(pszQuery) : nullptr;

    bAllowResetReadingEvenIfIndexAtZero = TRUE;

    OGRFeatureQuery oQuery;

    CPLPushErrorHandler(CPLQuietErrorHandler);
    int bHasSpecialFields = (pszQuery != nullptr && pszQuery[0] != '\0' &&
        oQuery.Compile( poLayer->GetLayerDefn(), pszQuery ) == OGRERR_NONE &&
        HasSpecialFields((swq_expr_node*)oQuery.GetSWQExpr(), poLayer->GetLayerDefn()->GetFieldCount()) );
    CPLPopErrorHandler();

    if( bHasSpecialFields || !BuildSQL() )
    {
        return poLayer->BaseSetAttributeFilter(pszQuery);
    }

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

GIntBig OGRSQLiteSelectLayer::GetFeatureCount( int bForce )
{
    return poBehavior->GetFeatureCount(bForce);
}

GIntBig OGRSQLiteSelectLayerCommonBehaviour::GetFeatureCount( int bForce )
{
    if( bEmptyLayer )
        return 0;

    if( poLayer->GetFeatureQuery() == nullptr &&
        STARTS_WITH_CI(osSQLCurrent, "SELECT COUNT(*) FROM") &&
        osSQLCurrent.ifind(" GROUP BY ") == std::string::npos &&
        osSQLCurrent.ifind(" UNION ") == std::string::npos &&
        osSQLCurrent.ifind(" INTERSECT ") == std::string::npos &&
        osSQLCurrent.ifind(" EXCEPT ") == std::string::npos )
        return 1;

    if( poLayer->GetFeatureQuery() != nullptr || (poLayer->GetFilterGeom() != nullptr && !bSpatialFilterInSQL) )
        return poLayer->BaseGetFeatureCount(bForce);

    CPLString osFeatureCountSQL("SELECT COUNT(*) FROM (");
    osFeatureCountSQL += osSQLCurrent;
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

    if( sqlite3_get_table( poDS->GetDB(), osFeatureCountSQL, &papszResult,
                           &nRowCount, &nColCount, &pszErrMsg ) != SQLITE_OK )
    {
        CPLDebug("SQLITE", "Error: %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return poLayer->BaseGetFeatureCount(bForce);
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

    iNextShapeId = 0;
    bDoStep = TRUE;

#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "prepare_v2(%s)", poBehavior->osSQLCurrent.c_str() );
#endif

    const int rc =
        sqlite3_prepare_v2( poDS->GetDB(), poBehavior->osSQLCurrent,
                         static_cast<int>(poBehavior->osSQLCurrent.size()),
                         &hStmt, nullptr );

    if( rc == SQLITE_OK )
        return OGRERR_NONE;

    CPLError( CE_Failure, CPLE_AppDefined,
              "In ResetStatement(): sqlite3_prepare_v2(%s):\n  %s",
              poBehavior->osSQLCurrent.c_str(), sqlite3_errmsg(poDS->GetDB()) );
    hStmt = nullptr;
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRSQLiteSelectLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    poBehavior->SetSpatialFilter(iGeomField, poGeomIn);
}

void OGRSQLiteSelectLayerCommonBehaviour::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField == 0 && poGeomIn == nullptr && poLayer->GetLayerDefn()->GetGeomFieldCount() == 0 )
    {
        /* do nothing */
    }
    else if( iGeomField < 0 || iGeomField >= poLayer->GetLayerDefn()->GetGeomFieldCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Invalid geometry field index : %d", iGeomField);
        return;
    }

    bAllowResetReadingEvenIfIndexAtZero = TRUE;

    int& m_iGeomFieldFilter = poLayer->GetIGeomFieldFilter();
    m_iGeomFieldFilter = iGeomField;
    if( poLayer->InstallFilter( poGeomIn ) )
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
    char** papszTokens = CSLTokenizeString(osSQLBase.c_str());
    int bCanInsertFilter = TRUE;
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
            bCanInsertFilter = FALSE;
        }
    }
    CSLDestroy(papszTokens);

    if (!(bCanInsertFilter && nCountSelect == 1 && nCountFrom == 1 && nCountWhere <= 1))
    {
        CPLDebug("SQLITE", "SQL expression too complex to analyse");
        return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>((OGRLayer*)nullptr, (IOGRSQLiteGetSpatialWhere*)nullptr);
    }

    size_t nFromPos = osSQLBase.ifind(" from ");
    if (nFromPos == std::string::npos)
    {
        return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>((OGRLayer*)nullptr, (IOGRSQLiteGetSpatialWhere*)nullptr);
    }

    /* Remove potential quotes around layer name */
    char chFirst = osSQLBase[nFromPos + 6];
    int bInQuotes = (chFirst == '\'' || chFirst == '"' );
    CPLString osBaseLayerName;
    for( i = nFromPos + 6 + (bInQuotes ? 1 : 0);
         i < osSQLBase.size(); i++ )
    {
        if (osSQLBase[i] == chFirst && bInQuotes )
        {
            if( i + 1 < osSQLBase.size() &&
                osSQLBase[i + 1] == chFirst )
            {
                osBaseLayerName += osSQLBase[i];
                i++;
            }
            else
            {
                i++;
                break;
            }
        }
        else if (osSQLBase[i] == ' ' && !bInQuotes)
            break;
        else
            osBaseLayerName += osSQLBase[i];
    }

    std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> oPair;
    if( strchr(osBaseLayerName, '(') == nullptr &&
        poLayer->GetLayerDefn()->GetGeomFieldCount() != 0 )
    {
        CPLString osNewUnderlyingTableName;
        osNewUnderlyingTableName.Printf("%s(%s)",
                                        osBaseLayerName.c_str(),
                                        poLayer->GetLayerDefn()->GetGeomFieldDefn(0)->GetNameRef());
        oPair = poDS->GetLayerWithGetSpatialWhereByName(osNewUnderlyingTableName);
    }
    if( oPair.first == nullptr )
        oPair = poDS->GetLayerWithGetSpatialWhereByName(osBaseLayerName);

    if( oPair.first != nullptr && poLayer->GetSpatialRef() != nullptr &&
        oPair.first->GetSpatialRef() != nullptr &&
        poLayer->GetSpatialRef() != oPair.first->GetSpatialRef() &&
        !poLayer->GetSpatialRef()->IsSame(oPair.first->GetSpatialRef()) )
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
    osSQLCurrent = osSQLBase;
    bSpatialFilterInSQL = TRUE;

    size_t i = 0;
    std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> oPair = GetBaseLayer(i);
    OGRLayer* poBaseLayer = oPair.first;
    if (poBaseLayer == nullptr)
    {
        CPLDebug("SQLITE", "Cannot find base layer");
        bSpatialFilterInSQL = FALSE;
        return FALSE;
    }

    CPLString osSpatialWhere;
    if (poLayer->GetFilterGeom() != nullptr)
    {
        const char* pszGeomCol =
            poLayer->GetLayerDefn()->GetGeomFieldDefn(poLayer->GetIGeomFieldFilter())->GetNameRef();
        int nIdx = poBaseLayer->GetLayerDefn()->GetGeomFieldIndex(pszGeomCol);
        if( nIdx < 0 )
        {
            CPLDebug("SQLITE", "Cannot find field %s in base layer", pszGeomCol);
            bSpatialFilterInSQL = FALSE;
        }
        else
        {
            osSpatialWhere = oPair.second->GetSpatialWhere(nIdx, poLayer->GetFilterGeom());
            if (osSpatialWhere.empty())
            {
                CPLDebug("SQLITE", "Cannot get spatial where clause");
                bSpatialFilterInSQL = FALSE;
            }
        }
    }

    CPLString osCustomWhere;
    if( !osSpatialWhere.empty() )
    {
        osCustomWhere = osSpatialWhere;
    }
    if( poLayer->GetAttrQueryString() != nullptr && poLayer->GetAttrQueryString()[0] != '\0' )
    {
        if( !osSpatialWhere.empty())
            osCustomWhere += " AND (";
        osCustomWhere += poLayer->GetAttrQueryString();
        if( !osSpatialWhere.empty())
            osCustomWhere += ")";
    }

    /* Nothing to do */
    if( osCustomWhere.empty() )
        return TRUE;

    while (i < osSQLBase.size() && osSQLBase[i] == ' ')
        i ++;

    if (i < osSQLBase.size() && STARTS_WITH_CI(osSQLBase.c_str() + i, "WHERE "))
    {
        osSQLCurrent = osSQLBase.substr(0, i + 6);
        osSQLCurrent += osCustomWhere;
        osSQLCurrent += " AND (";

        size_t nEndOfWhere = osSQLBase.ifind(" GROUP ");
        if (nEndOfWhere == std::string::npos)
            nEndOfWhere = osSQLBase.ifind(" ORDER ");
        if (nEndOfWhere == std::string::npos)
            nEndOfWhere = osSQLBase.ifind(" LIMIT ");

        if (nEndOfWhere == std::string::npos)
        {
            osSQLCurrent += osSQLBase.substr(i + 6);
            osSQLCurrent += ")";
        }
        else
        {
            osSQLCurrent += osSQLBase.substr(i + 6, nEndOfWhere - (i + 6));
            osSQLCurrent += ")";
            osSQLCurrent += osSQLBase.substr(nEndOfWhere);
        }
    }
    else if (i < osSQLBase.size() &&
             (STARTS_WITH_CI(osSQLBase.c_str() + i, "GROUP ") ||
              STARTS_WITH_CI(osSQLBase.c_str() + i, "ORDER ") ||
              STARTS_WITH_CI(osSQLBase.c_str() + i, "LIMIT ")))
    {
        osSQLCurrent = osSQLBase.substr(0, i);
        osSQLCurrent += " WHERE ";
        osSQLCurrent += osCustomWhere;
        osSQLCurrent += " ";
        osSQLCurrent += osSQLBase.substr(i);
    }
    else if (i == osSQLBase.size())
    {
        osSQLCurrent = osSQLBase.substr(0, i);
        osSQLCurrent += " WHERE ";
        osSQLCurrent += osCustomWhere;
    }
    else
    {
        CPLDebug("SQLITE", "SQL expression too complex for the driver to insert attribute and/or spatial filter in it");
        bSpatialFilterInSQL = FALSE;
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteSelectLayer::TestCapability( const char * pszCap )
{
    return poBehavior->TestCapability(pszCap);
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
        return poLayer->BaseTestCapability( pszCap );
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRSQLiteSelectLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    return poBehavior->GetExtent(iGeomField, psExtent, bForce);
}

OGRErr OGRSQLiteSelectLayerCommonBehaviour::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    if( iGeomField < 0 || iGeomField >= poLayer->GetLayerDefn()->GetGeomFieldCount() ||
        poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
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
        const OGREnvelope* psCachedExtent = poDS->GetEnvelopeFromSQL(osSQLBase);
        if (psCachedExtent)
        {
            *psExtent = *psCachedExtent;
            return OGRERR_NONE;
        }
    }

    CPLString osSQLCommand = osSQLBase;

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

        OGRLayer* poTmpLayer = poDS->ExecuteSQL(osSQLCommand.c_str(), nullptr, nullptr);
        if (poTmpLayer)
        {
            OGRErr eErr = poTmpLayer->GetExtent(iGeomField, psExtent, bForce);
            poDS->ReleaseResultSet(poTmpLayer);
            return eErr;
        }
    }

    OGRErr eErr;
    if( iGeomField == 0 )
        eErr = poLayer->BaseGetExtent(psExtent, bForce);
    else
        eErr = poLayer->BaseGetExtent(iGeomField, psExtent, bForce);
    if (iGeomField == 0 && eErr == OGRERR_NONE && poDS->GetUpdate() == FALSE)
        poDS->SetEnvelopeForSQL(osSQLBase, *psExtent);
    return eErr;
}
