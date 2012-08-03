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
                                            int bUseStatementForGetNextFeature )

{
    poDS = poDSIn;

    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = NULL;

    std::set<CPLString> aosEmpty;
    BuildFeatureDefn( "SELECT", hStmtIn, aosEmpty );

    if( bUseStatementForGetNextFeature )
    {
        hStmt = hStmtIn;
        bDoStep = FALSE;
    }
    else
        sqlite3_finalize( hStmtIn );

    osSQLBase = osSQLIn;
    osSQLCurrent = osSQLIn;
}

/************************************************************************/
/*                       ~OGRSQLiteSelectLayer()                        */
/************************************************************************/

OGRSQLiteSelectLayer::~OGRSQLiteSelectLayer()

{
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
        RebuildSQL();

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

    OGRSQLiteLayer* poUnderlyingLayer =
        (OGRSQLiteLayer*) poDS->GetLayerByName(osBaseLayerName);

    if( poUnderlyingLayer == NULL &&
        strchr(osBaseLayerName, '(') == NULL &&
        osGeomColumn.size() != 0 )
    {
        CPLString osNewUnderlyingTableName;
        osNewUnderlyingTableName.Printf("%s(%s)",
                                        osBaseLayerName.c_str(),
                                        osGeomColumn.c_str());
        poUnderlyingLayer =
            (OGRSQLiteLayer*) poDS->GetLayerByName(osNewUnderlyingTableName);
    }

    return poUnderlyingLayer;
}

/************************************************************************/
/*                            RebuildSQL()                              */
/************************************************************************/

void OGRSQLiteSelectLayer::RebuildSQL()

{
    osSQLCurrent = osSQLBase;

    if (m_poFilterGeom == NULL)
    {
        return;
    }

    size_t i = 0;
    OGRSQLiteLayer* poBaseLayer = GetBaseLayer(i);
    if (poBaseLayer == NULL)
    {
        CPLDebug("SQLITE", "Cannot find base layer");
        return;
    }

    CPLString    osSpatialWhere = poBaseLayer->GetSpatialWhere(m_poFilterGeom);
    if (osSpatialWhere.size() == 0)
    {
        CPLDebug("SQLITE", "Cannot get spatial where clause");
        return;
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
    }
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
