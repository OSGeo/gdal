/******************************************************************************
 * $Id$
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Implements OGRMSSQLSpatialSelectLayer class, layer access to the results
 *           of a SELECT statement executed via ExecuteSQL().
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
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
#include "ogr_mssqlspatial.h"

CPL_CVSID("$Id$");
/************************************************************************/
/*                     OGRMSSQLSpatialSelectLayer()                     */
/************************************************************************/

OGRMSSQLSpatialSelectLayer::OGRMSSQLSpatialSelectLayer( OGRMSSQLSpatialDataSource *poDSIn,
                                        CPLODBCStatement * poStmtIn )

{
    poDS = poDSIn;

    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = NULL;

    poStmt = poStmtIn;
    pszBaseStatement = CPLStrdup( poStmtIn->GetCommand() );

    /* identify the geometry column */
    pszGeomColumn = NULL;
    for ( int iColumn = 0; iColumn < poStmt->GetColCount(); iColumn++ )
    {
        if ( EQUAL(poStmt->GetColTypeName( iColumn ), "image") )
        {
            SQLCHAR     szTableName[256];
            SQLSMALLINT nTableNameLength = 0;

            SQLColAttribute(poStmt->GetStatement(), (SQLSMALLINT)(iColumn + 1), SQL_DESC_TABLE_NAME,
                                     szTableName, sizeof(szTableName),
                                     &nTableNameLength, NULL);

            if (nTableNameLength > 0)
            {
                OGRLayer *poBaseLayer = poDS->GetLayerByName((const char*)szTableName);
                if (poBaseLayer != NULL && EQUAL(poBaseLayer->GetGeometryColumn(), poStmt->GetColName(iColumn)))
                {
                    nGeomColumnType = MSSQLCOLTYPE_BINARY;
                    pszGeomColumn = CPLStrdup(poStmt->GetColName(iColumn));
                    /* copy spatial reference */
                    if (!poSRS && poBaseLayer->GetSpatialRef())
                        poSRS = poBaseLayer->GetSpatialRef()->Clone();
                    break;
                }
            }
        }
        else if ( EQUAL(poStmt->GetColTypeName( iColumn ), "geometry") )
        {
            nGeomColumnType = MSSQLCOLTYPE_GEOMETRY;
            pszGeomColumn = CPLStrdup(poStmt->GetColName(iColumn));
            break;
        }
        else if ( EQUAL(poStmt->GetColTypeName( iColumn ), "geography") )
        {
            nGeomColumnType = MSSQLCOLTYPE_GEOGRAPHY;
            pszGeomColumn = CPLStrdup(poStmt->GetColName(iColumn));
            break;
        }
    }

    BuildFeatureDefn( "SELECT", poStmt );
}

/************************************************************************/
/*                    ~OGRMSSQLSpatialSelectLayer()                     */
/************************************************************************/

OGRMSSQLSpatialSelectLayer::~OGRMSSQLSpatialSelectLayer()

{
    ClearStatement();
    CPLFree(pszBaseStatement);
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRMSSQLSpatialSelectLayer::ClearStatement()

{
    if( poStmt != NULL )
    {
        delete poStmt;
        poStmt = NULL;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRMSSQLSpatialSelectLayer::GetStatement()

{
    if( poStmt == NULL )
        ResetStatement();

    return poStmt;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRMSSQLSpatialSelectLayer::ResetStatement()

{
    ClearStatement();

    iNextShapeId = 0;

    CPLDebug( "OGR_MSSQLSpatial", "Recreating statement." );
    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( pszBaseStatement );

    if( poStmt->ExecuteSQL() )
        return OGRERR_NONE;
    else
    {
        delete poStmt;
        poStmt = NULL;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMSSQLSpatialSelectLayer::ResetReading()

{
    if( iNextShapeId != 0 )
        ClearStatement();

    OGRMSSQLSpatialLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRMSSQLSpatialSelectLayer::GetFeature( long nFeatureId )

{
    return OGRMSSQLSpatialLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMSSQLSpatialSelectLayer::TestCapability( const char * pszCap )

{
    return OGRMSSQLSpatialLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      Since SELECT layers currently cannot ever have geometry, we     */
/*      can optimize the GetExtent() method!                            */
/************************************************************************/

OGRErr OGRMSSQLSpatialSelectLayer::GetExtent(OGREnvelope *, int )

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

int OGRMSSQLSpatialSelectLayer::GetFeatureCount( int bForce )

{
    return OGRMSSQLSpatialLayer::GetFeatureCount( bForce );
}
