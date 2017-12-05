/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGeoTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_pgeo.h"
#include "ogrpgeogeometry.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRPGeoTableLayer()                         */
/************************************************************************/

OGRPGeoTableLayer::OGRPGeoTableLayer( OGRPGeoDataSource *poDSIn ) :
    pszQuery(NULL)
{
    poDS = poDSIn;
    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = NULL;
}

/************************************************************************/
/*                          ~OGRPGeoTableLayer()                          */
/************************************************************************/

OGRPGeoTableLayer::~OGRPGeoTableLayer()

{
    CPLFree( pszQuery );
    ClearStatement();
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRPGeoTableLayer::Initialize( const char *pszTableName,
                                      const char *pszGeomCol,
                                      int nShapeType,
                                      double dfExtentLeft,
                                      double dfExtentRight,
                                      double dfExtentBottom,
                                      double dfExtentTop,
                                      int nSRID,
                                      int bHasZ )

{
    CPLODBCSession *poSession = poDS->GetSession();

    SetDescription( pszTableName );

    CPLFree( pszGeomColumn );
    if( pszGeomCol == NULL )
        pszGeomColumn = NULL;
    else
        pszGeomColumn = CPLStrdup( pszGeomCol );

    CPLFree( pszFIDColumn );
    pszFIDColumn = NULL;

    sExtent.MinX = dfExtentLeft;
    sExtent.MaxX = dfExtentRight;
    sExtent.MinY = dfExtentBottom;
    sExtent.MaxY = dfExtentTop;

    LookupSRID( nSRID );

/* -------------------------------------------------------------------- */
/*      Setup geometry type.                                            */
/* -------------------------------------------------------------------- */
    OGRwkbGeometryType  eOGRType;

    switch( nShapeType )
    {
        case ESRI_LAYERGEOMTYPE_NULL:
            eOGRType = wkbNone;
            break;

        case ESRI_LAYERGEOMTYPE_POINT:
            eOGRType = wkbPoint;
            break;

        case ESRI_LAYERGEOMTYPE_MULTIPOINT:
            eOGRType = wkbMultiPoint;
            break;

        case ESRI_LAYERGEOMTYPE_POLYLINE:
            eOGRType = wkbLineString;
            break;

        case ESRI_LAYERGEOMTYPE_POLYGON:
        case ESRI_LAYERGEOMTYPE_MULTIPATCH:
            eOGRType = wkbPolygon;
            break;

        default:
            CPLDebug("PGeo", "Unexpected value for shape type : %d", nShapeType);
            eOGRType = wkbUnknown;
            break;
    }

    if( eOGRType != wkbUnknown && eOGRType != wkbNone && bHasZ )
        eOGRType = wkbSetZ(eOGRType);
    CPL_IGNORE_RET_VAL(eOGRType);

/* -------------------------------------------------------------------- */
/*      Do we have a simple primary key?                                */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetKey( poSession );

    if( oGetKey.GetPrimaryKeys( pszTableName ) && oGetKey.Fetch() )
    {
        pszFIDColumn = CPLStrdup(oGetKey.GetColData( 3 ));

        if( oGetKey.Fetch() ) // more than one field in key!
        {
            CPLFree( pszFIDColumn );
            pszFIDColumn = NULL;
            CPLDebug( "PGeo", "%s: Compound primary key, ignoring.",
                      pszTableName );
        }
        else
            CPLDebug( "PGeo",
                      "%s: Got primary key %s.",
                      pszTableName, pszFIDColumn );
    }
    else
        CPLDebug( "PGeo", "%s: no primary key", pszTableName );

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetCol( poSession );
    CPLErr eErr;

    if( !oGetCol.GetColumns( pszTableName ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GetColumns() failed on %s.\n%s",
                  pszTableName, poSession->GetLastError() );
        return CE_Failure;
    }

    eErr = BuildFeatureDefn( pszTableName, &oGetCol );
    if( eErr != CE_None )
        return eErr;

    if( poFeatureDefn->GetFieldCount() == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No column definitions found for table '%s', layer not usable.",
                  pszTableName );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Set geometry type.                                              */
/*                                                                      */
/*      NOTE: per reports from Craig Miller, it seems we cannot really  */
/*      trust the ShapeType value.  At the very least "line" tables     */
/*      sometimes have multilinestrings.  So for now we just always     */
/*      return wkbUnknown.                                              */
/*                                                                      */
/*      TODO - mloskot: Similar issue has been reported in Ticket #1484 */
/* -------------------------------------------------------------------- */
#ifdef notdef
    poFeatureDefn->SetGeomType( eOGRType );
#endif

    return CE_None;
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRPGeoTableLayer::ClearStatement()

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

CPLODBCStatement *OGRPGeoTableLayer::GetStatement()

{
    if( poStmt == NULL )
        ResetStatement();

    return poStmt;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRPGeoTableLayer::ResetStatement()

{
    ClearStatement();

    iNextShapeId = 0;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( poFeatureDefn->GetName() );
    if( pszQuery != NULL )
        poStmt->Appendf( " WHERE %s", pszQuery );

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

void OGRPGeoTableLayer::ResetReading()

{
    ClearStatement();
    OGRPGeoLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPGeoTableLayer::GetFeature( GIntBig nFeatureId )

{
    if( pszFIDColumn == NULL )
        return OGRPGeoLayer::GetFeature( nFeatureId );

    ClearStatement();

    iNextShapeId = nFeatureId;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( poFeatureDefn->GetName() );
    poStmt->Appendf( " WHERE %s = " CPL_FRMT_GIB, pszFIDColumn, nFeatureId );

    if( !poStmt->ExecuteSQL() )
    {
        delete poStmt;
        poStmt = NULL;
        return NULL;
    }

    return GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRPGeoTableLayer::SetAttributeFilter( const char *pszQueryIn )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQueryIn) ? CPLStrdup(pszQueryIn) : NULL;

    if( (pszQueryIn == NULL && pszQuery == NULL)
        || (pszQueryIn != NULL && pszQuery != NULL
            && EQUAL(pszQueryIn, pszQuery)) )
        return OGRERR_NONE;

    CPLFree( pszQuery );
    pszQuery = pszQueryIn ? CPLStrdup( pszQueryIn ) : NULL;

    ClearStatement();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGeoTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else
        return OGRPGeoLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGRPGeoTableLayer::GetFeatureCount( int bForce )

{
    if( m_poFilterGeom != NULL )
        return OGRPGeoLayer::GetFeatureCount( bForce );

    CPLODBCStatement oStmt( poDS->GetSession() );
    oStmt.Append( "SELECT COUNT(*) FROM " );
    oStmt.Append( poFeatureDefn->GetName() );

    if( pszQuery != NULL )
        oStmt.Appendf( " WHERE %s", pszQuery );

    if( !oStmt.ExecuteSQL() || !oStmt.Fetch() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GetFeatureCount() failed on query %s.\n%s",
                  oStmt.GetCommand(), poDS->GetSession()->GetLastError() );
        return OGRPGeoLayer::GetFeatureCount(bForce);
    }

    return CPLAtoGIntBig(oStmt.GetColData(0));
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRPGeoTableLayer::GetExtent( OGREnvelope *psExtent, CPL_UNUSED int bForce )
{
    *psExtent = sExtent;
    return OGRERR_NONE;
}
