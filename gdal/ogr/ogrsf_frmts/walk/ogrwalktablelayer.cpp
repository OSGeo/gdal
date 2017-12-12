/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRWalkTableLayer class, access to an existing table.
 * Author:   Xian Chen, chenxian at walkinfo.com.cn
 *
 ******************************************************************************
 * Copyright (c) 2013,  ZJU Walkinfo Technology Corp., Ltd.
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

#include "ogrwalk.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRWalkTableLayer()                         */
/************************************************************************/

OGRWalkTableLayer::OGRWalkTableLayer( OGRWalkDataSource *poDSIn ) :
    pszQuery(nullptr)
{
    poDS = poDSIn;

    iNextShapeId = 0;
    poFeatureDefn = nullptr;
}

/************************************************************************/
/*                         ~OGRWalkTableLayer()                         */
/************************************************************************/

OGRWalkTableLayer::~OGRWalkTableLayer()

{
    CPLFree( pszQuery );
    ClearStatement();
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRWalkTableLayer::Initialize( const char *pszLayerName,
                                      const char *pszGeomCol,
                                      double minE,
                                      double maxE,
                                      double minN,
                                      double maxN,
                                      const char *pszMemo)

{
    SetDescription( pszLayerName );

    CPLODBCSession *poSession = poDS->GetSession();

    CPLFree( pszFIDColumn );
    pszFIDColumn = nullptr;

    sExtent.MinX = minE;
    sExtent.MaxX = maxE;
    sExtent.MinY = minN;
    sExtent.MaxY = maxN;

/* -------------------------------------------------------------------- */
/*      Look up the Spatial Reference                                   */
/* -------------------------------------------------------------------- */
    LookupSpatialRef( pszMemo );

/* -------------------------------------------------------------------- */
/*      Generate the Feature Tablename from the Layer Name              */
/*      which is in the form <layername>Features                        */
/* -------------------------------------------------------------------- */
    char* pszFeatureTableName = (char *) CPLMalloc(strlen(pszLayerName)+10);

    snprintf(pszFeatureTableName, strlen(pszLayerName)+10, "%sFeatures", pszLayerName);

/* -------------------------------------------------------------------- */
/*      Do we have a simple primary key?                                */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetKey( poSession );

    if( oGetKey.GetPrimaryKeys( pszFeatureTableName, nullptr, nullptr )
        && oGetKey.Fetch() )
    {
        pszFIDColumn = CPLStrdup(oGetKey.GetColData( 3 ));

        if( oGetKey.Fetch() ) // more than one field in key!
        {
            CPLFree( pszFIDColumn );
            pszFIDColumn = nullptr;

            CPLDebug( "Walk", "Table %s has multiple primary key fields, "
                      "ignoring them all.", pszFeatureTableName );
        }
    }

/* -------------------------------------------------------------------- */
/*      Have we been provided a geometry column?                        */
/* -------------------------------------------------------------------- */
    CPLFree( pszGeomColumn );
    if( pszGeomCol == nullptr )
        pszGeomColumn = nullptr;
    else
        pszGeomColumn = CPLStrdup( pszGeomCol );

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetCol( poSession );
    CPLErr eErr;

    if( !oGetCol.GetColumns( pszFeatureTableName, nullptr, nullptr ) )
    {
        CPLFree( pszFeatureTableName );
        return CE_Failure;
    }

    eErr = BuildFeatureDefn( pszLayerName, &oGetCol );
    if( eErr != CE_None )
    {
        CPLFree( pszFeatureTableName );
        return eErr;
    }

    if( poFeatureDefn->GetFieldCount() == 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "No column definitions found for table '%s', layer not usable.",
                  pszLayerName );
        CPLFree( pszFeatureTableName );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      If we got a geometry column, does it exist?  Is it binary?      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != nullptr )
    {
        int iColumn = oGetCol.GetColId( pszGeomColumn );
        if( iColumn < 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Column %s requested for geometry, but it does not exist.",
                      pszGeomColumn );
            CPLFree( pszGeomColumn );
            pszGeomColumn = nullptr;
        }
        else
        {
            if( CPLODBCStatement::GetTypeMapping(
                    oGetCol.GetColType( iColumn )) == SQL_C_BINARY )
                bGeomColumnWKB = true;
        }
    }

    CPLFree( pszFeatureTableName );

    return CE_None;
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRWalkTableLayer::ClearStatement()

{
    if( poStmt != nullptr )
    {
        delete poStmt;
        poStmt = nullptr;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRWalkTableLayer::GetStatement()

{
    if( poStmt == nullptr )
        ResetStatement();

    return poStmt;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRWalkTableLayer::ResetStatement()

{
    ClearStatement();

    iNextShapeId = 0;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( poFeatureDefn->GetName() );
    poStmt->Append( "Features" );

    /* Append attribute query if we have it */
    if( (pszQuery != nullptr) && strcmp(pszQuery, "") )
        poStmt->Appendf( " WHERE %s", pszQuery );

    CPLDebug( "Walk", "ExecuteSQL(%s)", poStmt->GetCommand() );
    if( poStmt->ExecuteSQL() )
        return OGRERR_NONE;
    else
    {
        delete poStmt;
        poStmt = nullptr;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRWalkTableLayer::ResetReading()

{
    ClearStatement();
    OGRWalkLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRWalkTableLayer::GetFeature( GIntBig nFeatureId )

{
    if( pszFIDColumn == nullptr )
        return OGRWalkLayer::GetFeature( nFeatureId );

    ClearStatement();

    iNextShapeId = nFeatureId;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( poFeatureDefn->GetName() );
    poStmt->Append( "Features" );
    poStmt->Appendf( " WHERE %s = " CPL_FRMT_GIB, pszFIDColumn, nFeatureId );

    if( !poStmt->ExecuteSQL() )
    {
        delete poStmt;
        poStmt = nullptr;
        return nullptr;
    }

    return GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRWalkTableLayer::SetAttributeFilter( const char *pszQueryIn )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = pszQueryIn ? CPLStrdup(pszQueryIn) : nullptr;

    if( (pszQueryIn == nullptr && pszQuery == nullptr)
        || (pszQueryIn != nullptr && pszQuery != nullptr
            && EQUAL(pszQueryIn, pszQuery)) )
        return OGRERR_NONE;

    CPLFree( pszQuery );
    pszQuery = pszQueryIn != nullptr ? CPLStrdup( pszQueryIn ) : nullptr;

    ClearStatement();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWalkTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    return OGRWalkLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGRWalkTableLayer::GetFeatureCount( int bForce )

{
    if( m_poFilterGeom != nullptr )
        return OGRWalkLayer::GetFeatureCount( bForce );

    CPLODBCStatement oStmt( poDS->GetSession() );
    oStmt.Append( "SELECT COUNT(*) FROM " );
    oStmt.Append( poFeatureDefn->GetName() );
    oStmt.Append( "Features" );

    if( (pszQuery != nullptr) && strcmp(pszQuery, "") )
        oStmt.Appendf( " WHERE %s", pszQuery );

    if( !oStmt.ExecuteSQL() || !oStmt.Fetch() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GetFeatureCount() failed on query %s.\n%s",
                  oStmt.GetCommand(), poDS->GetSession()->GetLastError() );
        return OGRWalkLayer::GetFeatureCount(bForce);
    }

    return atoi(oStmt.GetColData(0));
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRWalkTableLayer::GetExtent( OGREnvelope *psExtent, CPL_UNUSED int bForce )
{
    *psExtent = sExtent;
    return OGRERR_NONE;
}
