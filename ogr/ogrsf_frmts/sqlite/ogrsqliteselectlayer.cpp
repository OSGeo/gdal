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

CPL_CVSID("$Id$");
/************************************************************************/
/*                        OGRSQLiteSelectLayer()                        */
/************************************************************************/

OGRSQLiteSelectLayer::OGRSQLiteSelectLayer( OGRSQLiteDataSource *poDSIn,
                                            CPLString osSQLIn,
                                            sqlite3_stmt *hStmtIn,
                                            int bUseStatementForGetNextFeature,
                                            int bEmptyLayer )

{
    poDS = poDSIn;

    iNextShapeId = 0;
    poFeatureDefn = NULL;

    std::set<CPLString> aosEmpty;
    BuildFeatureDefn( "SELECT", hStmtIn, aosEmpty );

    if( bUseStatementForGetNextFeature )
    {
        hStmt = hStmtIn;
        bDoStep = FALSE;

        // Try to extract SRS from first geometry
        if( !bEmptyLayer && osGeomColumn.size() != 0 )
        {
            int    nRawColumns = sqlite3_column_count( hStmt );
            for( int iCol = 0; iCol < nRawColumns; iCol++ )
            {
                int nBytes;
                if( sqlite3_column_type( hStmt, iCol ) == SQLITE_BLOB &&
                    strcmp(OGRSQLiteParamsUnquote(sqlite3_column_name( hStmt, iCol )).c_str(), osGeomColumn.c_str()) == 0 &&
                    (nBytes = sqlite3_column_bytes( hStmt, iCol )) > 39 )
                {
                    const GByte* pabyBlob = (const GByte*)sqlite3_column_blob( hStmt, iCol );
                    int eByteOrder = pabyBlob[1];
                    if( pabyBlob[0] == 0x00 &&
                        (eByteOrder == wkbNDR || eByteOrder == wkbXDR) &&
                        pabyBlob[38] == 0x7C )
                    {
                        memcpy(&nSRSId, pabyBlob + 2, 4);
#ifdef CPL_LSB
                        if( eByteOrder != wkbNDR)
                            CPL_SWAP32PTR(&nSRSId);
#else
                        if( eByteOrder == wkbNDR)
                            CPL_SWAP32PTR(&nSRSId);
#endif
                        poSRS = poDS->FetchSRS( nSRSId );
                        if( poSRS != NULL )
                            poSRS->Reference();
                    }
#if SQLITE_VERSION_NUMBER >= 3006010
                    else
                    {
                        const char* pszTableName = sqlite3_column_table_name( hStmt, iCol );
                        if( pszTableName != NULL )
                        {
                            OGRLayer* poLayer = poDS->GetLayerByName(pszTableName);
                            if( poLayer != NULL )
                            {
                                poSRS = poLayer->GetSpatialRef();
                                if( poSRS != NULL )
                                    poSRS->Reference();
                            }
                        }
                    }
#endif
                    break;
                }
            }
        }
    }
    else
        sqlite3_finalize( hStmtIn );

    osSQLBase = osSQLIn;
    osSQLCurrent = osSQLIn;
    this->bEmptyLayer = bEmptyLayer;
    bSpatialFilterInSQL = TRUE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSQLiteSelectLayer::GetNextFeature()
{
    if( bEmptyLayer )
        return NULL;

    return OGRSQLiteLayer::GetNextFeature();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

int OGRSQLiteSelectLayer::GetFeatureCount( int bForce )
{
    if( bEmptyLayer )
        return 0;

    if( m_poAttrQuery == NULL &&
        EQUALN(osSQLCurrent, "SELECT COUNT(*) FROM", strlen("SELECT COUNT(*) FROM")) &&
        osSQLCurrent.ifind(" GROUP BY ") == std::string::npos &&
        osSQLCurrent.ifind(" UNION ") == std::string::npos &&
        osSQLCurrent.ifind(" INTERSECT ") == std::string::npos &&
        osSQLCurrent.ifind(" EXCEPT ") == std::string::npos )
        return 1;

    if( m_poAttrQuery != NULL || (m_poFilterGeom != NULL && !bSpatialFilterInSQL) )
        return OGRLayer::GetFeatureCount(bForce);

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
        return OGRLayer::GetFeatureCount(bForce);
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
    CPLDebug( "OGR_SQLITE", "prepare(%s)", osSQLCurrent.c_str() );
#endif

    rc = sqlite3_prepare( poDS->GetDB(), osSQLCurrent, osSQLCurrent.size(),
                          &hStmt, NULL );

    if( rc == SQLITE_OK )
    {
        return OGRERR_NONE;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "In ResetStatement(): sqlite3_prepare(%s):\n  %s", 
                  osSQLCurrent.c_str(), sqlite3_errmsg(poDS->GetDB()) );
        hStmt = NULL;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRSQLiteSelectLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( InstallFilter( poGeomIn ) )
    {
        bSpatialFilterInSQL = RebuildSQLWithSpatialClause();

        ResetReading();
    }
}

/************************************************************************/
/*                            GetBaseLayer()                            */
/************************************************************************/

OGRSQLiteLayer* OGRSQLiteSelectLayer::GetBaseLayer(size_t& i)
{
    char** papszTokens = CSLTokenizeString(osSQLBase.c_str());
    int bCanInsertSpatialFilter = TRUE;
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
            bCanInsertSpatialFilter = FALSE;
        }
    }
    CSLDestroy(papszTokens);

    if (!(bCanInsertSpatialFilter && nCountSelect == 1 && nCountFrom == 1 && nCountWhere <= 1))
    {
        CPLDebug("SQLITE", "SQL expression too complex to analyse");
        return NULL;
    }

    size_t nFromPos = osSQLBase.ifind(" from ");
    if (nFromPos == std::string::npos)
    {
        return NULL;
    }

    int bInSingleQuotes = (osSQLBase[nFromPos + 6] == '\'');
    CPLString osBaseLayerName;
    for( i = nFromPos + 6 + (bInSingleQuotes ? 1 : 0);
         i < osSQLBase.size(); i++ )
    {
        if (osSQLBase[i] == '\'' && i + 1 < osSQLBase.size() &&
            osSQLBase[i + 1] == '\'' )
        {
            osBaseLayerName += osSQLBase[i];
            i++;
        }
        else if (osSQLBase[i] == '\'' && bInSingleQuotes)
        {
            i++;
            break;
        }
        else if (osSQLBase[i] == ' ' && !bInSingleQuotes)
            break;
        else
            osBaseLayerName += osSQLBase[i];
    }

    OGRSQLiteLayer* poUnderlyingLayer = NULL;
    if( strchr(osBaseLayerName, '(') == NULL &&
        osGeomColumn.size() != 0 )
    {
        CPLString osNewUnderlyingTableName;
        osNewUnderlyingTableName.Printf("%s(%s)",
                                        osBaseLayerName.c_str(),
                                        osGeomColumn.c_str());
        poUnderlyingLayer =
            (OGRSQLiteLayer*) poDS->GetLayerByName(osNewUnderlyingTableName);
    }
    if( poUnderlyingLayer == NULL )
        poUnderlyingLayer = (OGRSQLiteLayer*) poDS->GetLayerByName(osBaseLayerName);

    if( poUnderlyingLayer != NULL && poSRS != NULL &&
        poUnderlyingLayer->GetSpatialRef() != NULL &&
        poSRS != poUnderlyingLayer->GetSpatialRef() &&
        !poSRS->IsSame(poUnderlyingLayer->GetSpatialRef()) )
    {
        CPLDebug("SQLITE", "Result layer and base layer don't have the same SRS.");
        return NULL;
    }

    return poUnderlyingLayer;
}

/************************************************************************/
/*                    RebuildSQLWithSpatialClause()                     */
/************************************************************************/

int OGRSQLiteSelectLayer::RebuildSQLWithSpatialClause()

{
    osSQLCurrent = osSQLBase;

    if (m_poFilterGeom == NULL)
    {
        return TRUE;
    }

    size_t i = 0;
    OGRSQLiteLayer* poBaseLayer = GetBaseLayer(i);
    if (poBaseLayer == NULL)
    {
        CPLDebug("SQLITE", "Cannot find base layer");
        return FALSE;
    }

    CPLString    osSpatialWhere = poBaseLayer->GetSpatialWhere(m_poFilterGeom);
    if (osSpatialWhere.size() == 0)
    {
        CPLDebug("SQLITE", "Cannot get spatial where clause");
        return FALSE;
    }

    while (i < osSQLBase.size() && osSQLBase[i] == ' ')
        i ++;

    if (i < osSQLBase.size() && EQUALN(osSQLBase.c_str() + i, "WHERE ", 6))
    {
        osSQLCurrent = osSQLBase.substr(0, i + 6);
        osSQLCurrent += osSpatialWhere;
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
        osSQLCurrent += osSpatialWhere;
        osSQLCurrent += " ";
        osSQLCurrent += osSQLBase.substr(i);
    }
    else if (i == osSQLBase.size())
    {
        osSQLCurrent = osSQLBase.substr(0, i);
        osSQLCurrent += " WHERE ";
        osSQLCurrent += osSpatialWhere;
    }
    else
    {
        CPLDebug("SQLITE", "SQL expression too complex for the driver to insert spatial filter in it");
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteSelectLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap,OLCFastSpatialFilter))
    {
        if (osSQLCurrent != osSQLBase)
            return TRUE;

        size_t i = 0;
        OGRSQLiteLayer* poBaseLayer = GetBaseLayer(i);
        if (poBaseLayer == NULL)
        {
            CPLDebug("SQLITE", "Cannot find base layer");
            return FALSE;
        }

        OGRPolygon oFakePoly;
        const char* pszWKT = "POLYGON((0 0,0 1,1 1,1 0,0 0))";
        oFakePoly.importFromWkt((char**) &pszWKT);
        CPLString    osSpatialWhere = poBaseLayer->GetSpatialWhere(&oFakePoly);

        return osSpatialWhere.size() != 0;
    }
    else
        return OGRSQLiteLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRSQLiteSelectLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if (GetGeomType() == wkbNone)
        return OGRERR_FAILURE;

    /* Caching of extent by SQL string is interesting to speed-up the */
    /* establishment of the WFS GetCapabilities document for a MapServer mapfile */
    /* which has several layers, only differing by scale rules */
    const OGREnvelope* psCachedExtent = poDS->GetEnvelopeFromSQL(osSQLBase);
    if (psCachedExtent)
    {
        memcpy(psExtent, psCachedExtent, sizeof(*psCachedExtent));
        return OGRERR_NONE;
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
            OGRErr eErr = poTmpLayer->GetExtent(psExtent, bForce);
            poDS->ReleaseResultSet(poTmpLayer);
            return eErr;
        }
    }

    OGRErr eErr = OGRSQLiteLayer::GetExtent(psExtent, bForce);
    if (eErr == OGRERR_NONE && poDS->GetUpdate() == FALSE)
        poDS->SetEnvelopeForSQL(osSQLBase, *psExtent);
    return eErr;
}
