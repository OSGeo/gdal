/*****************************************************************************
 *
 * Project:  DB2 Spatial driver
 * Purpose:  Implements OGRDB2SelectLayer class, layer access to the results
 *           of a SELECT statement executed via ExecuteSQL().
 * Author:   David Adler, dadler at adtechgeospatial dot com
 *
 *****************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2015, David Adler
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
#include "ogr_db2.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                     OGRDB2SelectLayer()                     */
/************************************************************************/

OGRDB2SelectLayer::OGRDB2SelectLayer( OGRDB2DataSource *poDSIn,
                                      OGRDB2Statement * poStmtIn )

{

    SQLCHAR     szTableName[256];
    SQLCHAR     szSchemaName[256];
    SQLSMALLINT nNameLength = 0;
    OGRDB2Layer *poBaseLayer = nullptr;
    poDS = poDSIn;

    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = nullptr;

    m_poStmt = poStmtIn;
    pszBaseStatement = CPLStrdup( poStmtIn->GetCommand() );
    CPLDebug("OGR_DB2SelectLayer::OGRDB2SelectLayer", "SQL: '%s'",
             pszBaseStatement);

    pszGeomColumn = nullptr;

    /* get schema and table names for first column, column 1 */
    SQLColAttribute(m_poStmt->GetStatement(), (SQLSMALLINT)(1),
                    SQL_DESC_SCHEMA_NAME,
                    szSchemaName, sizeof(szSchemaName),
                    &nNameLength, nullptr);
    /* The schema name is sometimes right padded with blanks */
    /* Replace blanks with nulls to terminate string for sprintf below */
    for (int i = 0; i < nNameLength; i++) {
        if (szSchemaName[i] == ' ') szSchemaName[i] = 0;
    };
    SQLColAttribute(m_poStmt->GetStatement(), (SQLSMALLINT)(1),
                    SQL_DESC_TABLE_NAME,
                    szTableName, sizeof(szTableName),
                    &nNameLength, nullptr);
    CPLDebug("OGR_DB2SelectLayer::OGRDB2SelectLayer",
             "szSchemaName: '%s'; szTableName: '%s'",
             szSchemaName, szTableName);
    if (nNameLength > 0)
    {
        char szLayerName[512];
        snprintf(szLayerName, sizeof(szLayerName), "%s.%s",szSchemaName, szTableName);
        poBaseLayer = (OGRDB2Layer *) poDS->GetLayerByName((const char*)
                      szLayerName);
        if (poBaseLayer != nullptr)
            CPLDebug("OGR_DB2SelectLayer::OGRDB2SelectLayer",
                     "base geom col: '%s'", poBaseLayer->GetGeometryColumn());
        else CPLDebug("OGR_DB2SelectLayer::OGRDB2SelectLayer",
                          "base layer not found");
    }

    /* identify the geometry column */
    for ( int iColumn = 0; iColumn < m_poStmt->GetColCount(); iColumn++ )
    {
        if ( EQUAL(m_poStmt->GetColTypeName( iColumn ), "CLOB") ||
                EQUAL(m_poStmt->GetColTypeName( iColumn ),
                      "VARCHAR () FOR BIT DATA"))
        {
            if (poBaseLayer != nullptr
                    && EQUAL(poBaseLayer->GetGeometryColumn(),
                             m_poStmt->GetColName(iColumn)))
            {
                pszGeomColumn = CPLStrdup(m_poStmt->GetColName(iColumn));
                /* copy spatial reference */
                if (!poSRS && poBaseLayer->GetSpatialRef())
                    poSRS = poBaseLayer->GetSpatialRef()->Clone();
                nSRSId = poBaseLayer->GetSRSId();
                break;
            }
        }
    }

    BuildFeatureDefn( "SELECT", m_poStmt );

    if ( GetSpatialRef() && poFeatureDefn->GetGeomFieldCount() == 1)
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef( poSRS );
}

/************************************************************************/
/*                    ~OGRDB2SelectLayer()                     */
/************************************************************************/

OGRDB2SelectLayer::~OGRDB2SelectLayer()

{
    ClearStatement();
    CPLFree(pszBaseStatement);
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRDB2SelectLayer::ClearStatement()

{
    if( m_poStmt != nullptr )
    {
        delete m_poStmt;
        m_poStmt = nullptr;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

OGRDB2Statement *OGRDB2SelectLayer::GetStatement()

{
    if( m_poStmt == nullptr )
        ResetStatement();

    return m_poStmt;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRDB2SelectLayer::ResetStatement()

{
    ClearStatement();

    iNextShapeId = 0;

    CPLDebug( "OGR_DB2SelectLayer::ResetStatement", "Recreating statement." );
    m_poStmt = new OGRDB2Statement( poDS->GetSession() );
    m_poStmt->Append( pszBaseStatement );

    if( m_poStmt->ExecuteSQL() )
        return OGRERR_NONE;
    else
    {
        delete m_poStmt;
        m_poStmt = nullptr;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDB2SelectLayer::ResetReading()

{
    if( iNextShapeId != 0 )
        ClearStatement();

    OGRDB2Layer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRDB2SelectLayer::GetFeature( GIntBig nFeatureId )

{
    return OGRDB2Layer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDB2SelectLayer::TestCapability( const char * pszCap )

{
    return OGRDB2Layer::TestCapability( pszCap );
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      Since SELECT layers currently cannot ever have geometry, we     */
/*      can optimize the GetExtent() method!                            */
/************************************************************************/

OGRErr OGRDB2SelectLayer::GetExtent(OGREnvelope *, int )

{
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGRDB2SelectLayer::GetFeatureCount( int bForce )

{
    return OGRDB2Layer::GetFeatureCount( bForce );
}
