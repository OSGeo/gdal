/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGeoTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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

OGRPGeoTableLayer::OGRPGeoTableLayer(OGRPGeoDataSource *poDSIn, int nODBCStatementFlags) :
    pszQuery(nullptr)
{
    m_nStatementFlags = nODBCStatementFlags;
    poDS = poDSIn;
    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = nullptr;
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
                                      int bHasZ,
                                      int bHasM )

{
    CPLODBCSession *poSession = poDS->GetSession();

    SetDescription( pszTableName );

    CPLFree( pszGeomColumn );
    if( pszGeomCol == nullptr )
        pszGeomColumn = nullptr;
    else
        pszGeomColumn = CPLStrdup( pszGeomCol );

    CPLFree( pszFIDColumn );
    pszFIDColumn = nullptr;

    sExtent.MinX = dfExtentLeft;
    sExtent.MaxX = dfExtentRight;
    sExtent.MinY = dfExtentBottom;
    sExtent.MaxY = dfExtentTop;

    if ( pszGeomCol )
        LookupSRID( nSRID );

    // Setup geometry type.

    // The PGeo format has a similar approach to multi-part handling as Shapefiles,
    // where polygon and multipolygon geometries or line and multiline geometries will
    // co-exist in a layer reported as just polygon or line type respectively.
    // To handle this in a predictable way for clients we always promote the polygon/line
    // types to multitypes, and correspondingly ALWAYS return multi polygon/line geometry
    // objects for features (even if strictly speaking the original feature had a polygon/line
    // geometry object)
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
            eOGRType = wkbMultiLineString;  // see comment above
            break;

        case ESRI_LAYERGEOMTYPE_POLYGON:
        case ESRI_LAYERGEOMTYPE_MULTIPATCH:
            eOGRType = wkbMultiPolygon; // see comment above
            break;

        default:
            CPLDebug("PGeo", "Unexpected value for shape type : %d", nShapeType);
            eOGRType = wkbUnknown;
            break;
    }

    if( eOGRType != wkbUnknown && eOGRType != wkbNone )
    {
        if ( bHasZ )
          eOGRType = wkbSetZ(eOGRType);
        if ( bHasM )
          eOGRType = wkbSetM(eOGRType);
    }
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
            pszFIDColumn = nullptr;
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

    poFeatureDefn->SetGeomType( eOGRType );

    // Where possible, retrieve useful metadata information from the GDB_Items table
    if ( poDS->HasGdbItemsTable() )
    {
        CPLODBCStatement oItemsStmt( poSession );
        oItemsStmt.Append( "SELECT Definition, Documentation FROM GDB_Items WHERE Name='" );
        oItemsStmt.Append( pszTableName );
        oItemsStmt.Append( "'" );
        if( oItemsStmt.ExecuteSQL() )
        {
            while( oItemsStmt.Fetch() )
            {
                const CPLString osDefinition = CPLString( oItemsStmt.GetColData(0, "") );
                if( strstr(osDefinition, "DEFeatureClassInfo") != nullptr )
                {
                    m_osDefinition = osDefinition;

                    // try to retrieve field domains
                    CPLXMLTreeCloser oTree(CPLParseXMLString(osDefinition.c_str()));
                    if( oTree.get() )
                    {
                        if ( const CPLXMLNode* psFieldInfoExs = CPLGetXMLNode(oTree.get(), "=DEFeatureClassInfo.GPFieldInfoExs") )
                        {
                            for( const CPLXMLNode *psFieldInfoEx =
                                     CPLGetXMLNode( psFieldInfoExs, "GPFieldInfoEx" );
                                 psFieldInfoEx != nullptr;
                                 psFieldInfoEx = psFieldInfoEx->psNext )
                            {
                                const CPLString osName = CPLGetXMLValue(psFieldInfoEx, "Name", "");
                                const CPLString osDomainName = CPLGetXMLValue(psFieldInfoEx, "DomainName", "");
                                if ( !osDomainName.empty() )
                                {
                                    const int fieldIndex = poFeatureDefn->GetFieldIndex( osName );
                                    if ( fieldIndex != -1 )
                                        poFeatureDefn->GetFieldDefn( fieldIndex )->SetDomainName( osDomainName );
                                }
                            }
                        }
                    }

                    // try to retrieve layer medata
                    m_osDocumentation = CPLString( oItemsStmt.GetColData(1, "") );
                    break;
                }
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRPGeoTableLayer::ClearStatement()

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

CPLODBCStatement *OGRPGeoTableLayer::GetStatement()

{
    if( poStmt == nullptr )
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

    poStmt = new CPLODBCStatement( poDS->GetSession(), m_nStatementFlags );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( poFeatureDefn->GetName() );
    if( pszQuery != nullptr )
        poStmt->Appendf( " WHERE %s", pszQuery );

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
    if( pszFIDColumn == nullptr )
        return OGRPGeoLayer::GetFeature( nFeatureId );

    ClearStatement();

    iNextShapeId = nFeatureId;

    poStmt = new CPLODBCStatement( poDS->GetSession(), m_nStatementFlags );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( poFeatureDefn->GetName() );
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

OGRErr OGRPGeoTableLayer::SetAttributeFilter( const char *pszQueryIn )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQueryIn) ? CPLStrdup(pszQueryIn) : nullptr;

    if( (pszQueryIn == nullptr && pszQuery == nullptr)
        || (pszQueryIn != nullptr && pszQuery != nullptr
            && EQUAL(pszQueryIn, pszQuery)) )
        return OGRERR_NONE;

    CPLFree( pszQuery );
    pszQuery = pszQueryIn ? CPLStrdup( pszQueryIn ) : nullptr;

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
        return m_poFilterGeom == nullptr && poDS->CountStarWorking();

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
    if( m_poFilterGeom != nullptr || !poDS->CountStarWorking() )
        return OGRPGeoLayer::GetFeatureCount( bForce );

    CPLODBCStatement oStmt( poDS->GetSession() );
    oStmt.Append( "SELECT COUNT(*) FROM " );
    oStmt.Append( poFeatureDefn->GetName() );

    if( pszQuery != nullptr )
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
    if( pszGeomColumn == nullptr )
    {
        return OGRERR_FAILURE;
    }

    *psExtent = sExtent;
    return OGRERR_NONE;
}
