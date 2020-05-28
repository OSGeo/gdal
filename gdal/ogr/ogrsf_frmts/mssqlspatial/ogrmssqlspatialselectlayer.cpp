/******************************************************************************
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

// SQL_CA_SS_UDT_TYPE_NAME not defined in unixODBC headers
#ifndef SQL_CA_SS_BASE
#define SQL_CA_SS_BASE 1200
#endif

#ifndef SQL_CA_SS_UDT_TYPE_NAME
#define SQL_CA_SS_UDT_TYPE_NAME (SQL_CA_SS_BASE+20)
#endif

CPL_CVSID("$Id$")
/************************************************************************/
/*                     OGRMSSQLSpatialSelectLayer()                     */
/************************************************************************/

OGRMSSQLSpatialSelectLayer::OGRMSSQLSpatialSelectLayer( OGRMSSQLSpatialDataSource *poDSIn,
                                        CPLODBCStatement * poStmtIn )

{
    poDS = poDSIn;

    iNextShapeId = 0;
    nSRSId = 0;
    poFeatureDefn = nullptr;

    poStmt = poStmtIn;
    pszBaseStatement = CPLStrdup( poStmtIn->GetCommand() );

    /* identify the geometry column */
    pszGeomColumn = nullptr;
    int iImageCol = -1;
    for ( int iColumn = 0; iColumn < poStmt->GetColCount(); iColumn++ )
    {
        if ( EQUAL(poStmt->GetColTypeName( iColumn ), "image") )
        {
            SQLCHAR     szTableName[256];
            SQLSMALLINT nTableNameLength = 0;

            SQLColAttribute(poStmt->GetStatement(), (SQLSMALLINT)(iColumn + 1), SQL_DESC_TABLE_NAME,
                                     szTableName, sizeof(szTableName),
                                     &nTableNameLength, nullptr);

            if (nTableNameLength > 0)
            {
                OGRLayer *poBaseLayer = poDS->GetLayerByName((const char*)szTableName);
                if (poBaseLayer != nullptr && EQUAL(poBaseLayer->GetGeometryColumn(), poStmt->GetColName(iColumn)))
                {
                    nGeomColumnType = MSSQLCOLTYPE_BINARY;
                    pszGeomColumn = CPLStrdup(poStmt->GetColName(iColumn));
                    /* copy spatial reference */
                    if (!poSRS && poBaseLayer->GetSpatialRef())
                        poSRS = poBaseLayer->GetSpatialRef()->Clone();
                    break;
                }
            }
            else if (iImageCol == -1)
                iImageCol = iColumn;
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
        else if ( EQUAL(poStmt->GetColTypeName( iColumn ), "udt") )
        {
            SQLCHAR     szUDTTypeName[256];
            SQLSMALLINT nUDTTypeNameLength = 0;

            SQLColAttribute(poStmt->GetStatement(), (SQLSMALLINT)(iColumn + 1), SQL_CA_SS_UDT_TYPE_NAME,
                                     szUDTTypeName, sizeof(szUDTTypeName),
                                     &nUDTTypeNameLength, nullptr);

            // For some reason on unixODBC, a UCS2 string is returned
            if ( EQUAL((char*)szUDTTypeName, "geometry") ||
                 (nUDTTypeNameLength == 16 &&
                  memcmp(szUDTTypeName, "g\0e\0o\0m\0e\0t\0r\0y", 16) == 0) )
            {
                nGeomColumnType = MSSQLCOLTYPE_GEOMETRY;
                pszGeomColumn = CPLStrdup(poStmt->GetColName(iColumn));
            }
            else if ( EQUAL((char*)szUDTTypeName, "geography") ||
                 (nUDTTypeNameLength == 18 &&
                  memcmp(szUDTTypeName, "g\0e\0o\0g\0r\0a\0p\0h\0y", 18) == 0) )
            {
                nGeomColumnType = MSSQLCOLTYPE_GEOGRAPHY;
                pszGeomColumn = CPLStrdup(poStmt->GetColName(iColumn));
            }
            break;
        }
    }

    if (pszGeomColumn == nullptr && iImageCol >= 0)
    {
        /* set the image col as geometry column as the last resort */
        nGeomColumnType = MSSQLCOLTYPE_BINARY;
        pszGeomColumn = CPLStrdup(poStmt->GetColName(iImageCol));
    }

    BuildFeatureDefn( "SELECT", poStmt );

    if ( GetSpatialRef() && poFeatureDefn->GetGeomFieldCount() == 1)
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef( poSRS );
}

/************************************************************************/
/*                    ~OGRMSSQLSpatialSelectLayer()                     */
/************************************************************************/

OGRMSSQLSpatialSelectLayer::~OGRMSSQLSpatialSelectLayer()

{
    CPLFree(pszBaseStatement);
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRMSSQLSpatialSelectLayer::GetStatement()

{
    if( poStmt == nullptr )
    {
        CPLDebug( "OGR_MSSQLSpatial", "Recreating statement." );
        poStmt = new CPLODBCStatement( poDS->GetSession() );
        poStmt->Append( pszBaseStatement );

        if( !poStmt->ExecuteSQL() )
        {
            delete poStmt;
            poStmt = nullptr;
        }
    }

    return poStmt;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRMSSQLSpatialSelectLayer::GetFeature( GIntBig nFeatureId )

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

GIntBig OGRMSSQLSpatialSelectLayer::GetFeatureCount( int bForce )

{
    return OGRMSSQLSpatialLayer::GetFeatureCount( bForce );
}
