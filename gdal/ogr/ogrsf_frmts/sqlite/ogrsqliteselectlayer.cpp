/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteSelectLayer class, layer access to the results
 *           of a SELECT statement executed via ExecuteSQL().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_sqlite.h"
#include "swq.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                   OGRSQLiteSelectLayerCommonBehaviour()              */
/************************************************************************/

OGRSQLiteSelectLayerCommonBehaviour::OGRSQLiteSelectLayerCommonBehaviour(OGRSQLiteBaseDataSource* poDS,
                                            IOGRSQLiteSelectLayer* poLayer,
                                            CPLString osSQL,
                                            int bEmptyLayer) :
            poDS(poDS), poLayer(poLayer), osSQLBase(osSQL),
            bEmptyLayer(bEmptyLayer), osSQLCurrent(osSQL)
{
    bAllowResetReadingEvenIfIndexAtZero = FALSE;
    bSpatialFilterInSQL = TRUE;
}

/************************************************************************/
/*                        OGRSQLiteSelectLayer()                        */
/************************************************************************/

OGRSQLiteSelectLayer::OGRSQLiteSelectLayer( OGRSQLiteDataSource *poDSIn,
                                            CPLString osSQLIn,
                                            sqlite3_stmt *hStmtIn,
                                            int bUseStatementForGetNextFeature,
                                            int bEmptyLayer,
                                            int bAllowMultipleGeomFields )

{
    poBehaviour = new OGRSQLiteSelectLayerCommonBehaviour(poDSIn, this, osSQLIn, bEmptyLayer);
    poDS = poDSIn;

    this->bAllowMultipleGeomFields = bAllowMultipleGeomFields;

    std::set<CPLString> aosEmpty;
    BuildFeatureDefn( "SELECT", hStmtIn, NULL, aosEmpty );
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

            int nBytes;
            if( sqlite3_column_type( hStmt, poGeomFieldDefn->iCol ) == SQLITE_BLOB &&
                (nBytes = sqlite3_column_bytes( hStmt, poGeomFieldDefn->iCol )) > 39 )
            {
                const GByte* pabyBlob = (const GByte*)sqlite3_column_blob( hStmt, poGeomFieldDefn->iCol );
                int eByteOrder = pabyBlob[1];
                if( pabyBlob[0] == 0x00 &&
                    (eByteOrder == wkbNDR || eByteOrder == wkbXDR) &&
                    pabyBlob[38] == 0x7C )
                {
                    int nSRSId;
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
                    if( poSRS != NULL )
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
                    if( pszTableName != NULL )
                    {
                        OGRSQLiteLayer* poLayer = (OGRSQLiteLayer*)
                                        poDS->GetLayerByName(pszTableName);
                        if( poLayer != NULL &&  poLayer->GetLayerDefn()->GetGeomFieldCount() > 0)
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
    delete poBehaviour;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSQLiteSelectLayer::ResetReading()
{
    return poBehaviour->ResetReading();
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
    return poBehaviour->GetNextFeature();
}

OGRFeature *OGRSQLiteSelectLayerCommonBehaviour::GetNextFeature()
{
    if( bEmptyLayer )
        return NULL;

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
    return poBehaviour->SetAttributeFilter(pszQuery);
}

OGRErr OGRSQLiteSelectLayerCommonBehaviour::SetAttributeFilter( const char *pszQuery )

{
    char*& m_pszAttrQuertyString = poLayer->GetAttrQueryString();
    if( m_pszAttrQuertyString == NULL && pszQuery == NULL )
        return OGRERR_NONE;

    CPLFree(m_pszAttrQuertyString);
    m_pszAttrQuertyString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

    bAllowResetReadingEvenIfIndexAtZero = TRUE;

    OGRFeatureQuery oQuery;

    CPLPushErrorHandler(CPLQuietErrorHandler);
    int bHasSpecialFields = (pszQuery != NULL && pszQuery[0] != '\0' &&
        oQuery.Compile( poLayer->GetLayerDefn(), pszQuery ) == OGRERR_NONE &&
        HasSpecialFields((swq_expr_node*)oQuery.GetSWGExpr(), poLayer->GetLayerDefn()->GetFieldCount()) );
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
    return poBehaviour->GetFeatureCount(bForce);
}

GIntBig OGRSQLiteSelectLayerCommonBehaviour::GetFeatureCount( int bForce )
{
    if( bEmptyLayer )
        return 0;

    if( poLayer->GetFeatureQuery() == NULL &&
        EQUALN(osSQLCurrent, "SELECT COUNT(*) FROM", strlen("SELECT COUNT(*) FROM")) &&
        osSQLCurrent.ifind(" GROUP BY ") == std::string::npos &&
        osSQLCurrent.ifind(" UNION ") == std::string::npos &&
        osSQLCurrent.ifind(" INTERSECT ") == std::string::npos &&
        osSQLCurrent.ifind(" EXCEPT ") == std::string::npos )
        return 1;

    if( poLayer->GetFeatureQuery() != NULL || (poLayer->GetFilterGeom() != NULL && !bSpatialFilterInSQL) )
        return poLayer->BaseGetFeatureCount(bForce);

    CPLString osFeatureCountSQL("SELECT COUNT(*) FROM (");
    osFeatureCountSQL += osSQLCurrent;
    osFeatureCountSQL += ")";

    CPLDebug("SQLITE", "Running %s", osFeatureCountSQL.c_str());

/* -------------------------------------------------------------------- */
/*      Execute.                                                        */
/* -------------------------------------------------------------------- */
    char *pszErrMsg = NULL;
    char **papszResult;
    int nRowCount, nColCount;
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
    int rc;

    ClearStatement();

    iNextShapeId = 0;
    bDoStep = TRUE;

#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "prepare(%s)", poBehaviour->osSQLCurrent.c_str() );
#endif

    rc = sqlite3_prepare( poDS->GetDB(), poBehaviour->osSQLCurrent, poBehaviour->osSQLCurrent.size(),
                          &hStmt, NULL );

    if( rc == SQLITE_OK )
    {
        return OGRERR_NONE;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "In ResetStatement(): sqlite3_prepare(%s):\n  %s", 
                  poBehaviour->osSQLCurrent.c_str(), sqlite3_errmsg(poDS->GetDB()) );
        hStmt = NULL;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRSQLiteSelectLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    poBehaviour->SetSpatialFilter(iGeomField, poGeomIn);
}

void OGRSQLiteSelectLayerCommonBehaviour::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField == 0 && poGeomIn == NULL && poLayer->GetLayerDefn()->GetGeomFieldCount() == 0 )
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

    for(int iToken = 0; papszTokens[iToken] != NULL; iToken++)
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
        return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>((OGRLayer*)NULL, (IOGRSQLiteGetSpatialWhere*)NULL);
    }

    size_t nFromPos = osSQLBase.ifind(" from ");
    if (nFromPos == std::string::npos)
    {
        return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>((OGRLayer*)NULL, (IOGRSQLiteGetSpatialWhere*)NULL);
    }

    char chQuote = osSQLBase[nFromPos + 6];
    int bInQuotes = (chQuote == '\'' || chQuote == '"' );
    CPLString osBaseLayerName;
    for( i = nFromPos + 6 + (bInQuotes ? 1 : 0);
         i < osSQLBase.size(); i++ )
    {
        if (osSQLBase[i] == chQuote && i + 1 < osSQLBase.size() &&
            osSQLBase[i + 1] == chQuote )
        {
            osBaseLayerName += osSQLBase[i];
            i++;
        }
        else if (osSQLBase[i] == chQuote && bInQuotes)
        {
            i++;
            break;
        }
        else if (osSQLBase[i] == ' ' && !bInQuotes)
            break;
        else
            osBaseLayerName += osSQLBase[i];
    }

    std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> oPair;
    if( strchr(osBaseLayerName, '(') == NULL &&
        poLayer->GetLayerDefn()->GetGeomFieldCount() != 0 )
    {
        CPLString osNewUnderlyingTableName;
        osNewUnderlyingTableName.Printf("%s(%s)",
                                        osBaseLayerName.c_str(),
                                        poLayer->GetLayerDefn()->GetGeomFieldDefn(0)->GetNameRef());
        oPair = poDS->GetLayerWithGetSpatialWhereByName(osNewUnderlyingTableName);
    }
    if( oPair.first == NULL )
        oPair = poDS->GetLayerWithGetSpatialWhereByName(osBaseLayerName);

    if( oPair.first != NULL && poLayer->GetSpatialRef() != NULL &&
        oPair.first->GetSpatialRef() != NULL &&
        poLayer->GetSpatialRef() != oPair.first->GetSpatialRef() &&
        !poLayer->GetSpatialRef()->IsSame(oPair.first->GetSpatialRef()) )
    {
        CPLDebug("SQLITE", "Result layer and base layer don't have the same SRS.");
        return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>((OGRLayer*)NULL, (IOGRSQLiteGetSpatialWhere*)NULL);
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
    if (poBaseLayer == NULL)
    {
        CPLDebug("SQLITE", "Cannot find base layer");
        bSpatialFilterInSQL = FALSE;
        return FALSE;
    }

    CPLString osSpatialWhere;
    if (poLayer->GetFilterGeom() != NULL)
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
            if (osSpatialWhere.size() == 0)
            {
                CPLDebug("SQLITE", "Cannot get spatial where clause");
                bSpatialFilterInSQL = FALSE;
            }
        }
    }

    CPLString osCustomWhere;
    if( osSpatialWhere.size() != 0 )
    {
        osCustomWhere = osSpatialWhere;
    }
    if( poLayer->GetAttrQueryString() != NULL && poLayer->GetAttrQueryString()[0] != '\0' )
    {
        if( osSpatialWhere.size() != 0)
            osCustomWhere += " AND (";
        osCustomWhere += poLayer->GetAttrQueryString();
        if( osSpatialWhere.size() != 0)
            osCustomWhere += ")";
    }

    /* Nothing to do */
    if( osCustomWhere.size() == 0 )
        return TRUE;

    while (i < osSQLBase.size() && osSQLBase[i] == ' ')
        i ++;

    if (i < osSQLBase.size() && EQUALN(osSQLBase.c_str() + i, "WHERE ", 6))
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
             (EQUALN(osSQLBase.c_str() + i, "GROUP ", 6) ||
              EQUALN(osSQLBase.c_str() + i, "ORDER ", 6) ||
              EQUALN(osSQLBase.c_str() + i, "LIMIT ", 6)))
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
    return poBehaviour->TestCapability(pszCap);
}

int OGRSQLiteSelectLayerCommonBehaviour::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap,OLCFastSpatialFilter))
    {
        size_t i = 0;
        std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> oPair = GetBaseLayer(i);
        if (oPair.first == NULL)
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
    return poBehaviour->GetExtent(iGeomField, psExtent, bForce);
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
            memcpy(psExtent, psCachedExtent, sizeof(*psCachedExtent));
            return OGRERR_NONE;
        }
    }

    CPLString osSQLCommand = osSQLBase;

    /* ORDER BY are costly to evaluate and are not necessary to establish */
    /* the layer extent. */
    size_t nOrderByPos = osSQLCommand.ifind(" ORDER BY ");
    if( osSQLCommand.ifind("SELECT ") == 0 &&
        nOrderByPos != std::string::npos &&
        osSQLCommand.ifind(" LIMIT ") == std::string::npos &&
        osSQLCommand.ifind(" UNION ") == std::string::npos &&
        osSQLCommand.ifind(" INTERSECT ") == std::string::npos &&
        osSQLCommand.ifind(" EXCEPT ") == std::string::npos)
    {
        osSQLCommand.resize(nOrderByPos);

        OGRLayer* poTmpLayer = poDS->ExecuteSQL(osSQLCommand.c_str(), NULL, NULL);
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
