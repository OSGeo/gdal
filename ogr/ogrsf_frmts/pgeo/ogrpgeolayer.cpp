/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGeoLayer class, code shared between
 *           the direct table access, and the generic SQL results.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_string.h"
#include "ogrpgeogeometry.h"

#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRPGeoLayer()                            */
/************************************************************************/

OGRPGeoLayer::OGRPGeoLayer() :
    poFeatureDefn(nullptr),
    poStmt(nullptr),
    poSRS(nullptr),
    nSRSId(-2), // we haven't even queried the database for it yet.
    iNextShapeId(0),
    poDS(nullptr),
    pszGeomColumn(nullptr),
    pszFIDColumn(nullptr),
    panFieldOrdinals(nullptr)
{}

/************************************************************************/
/*                            ~OGRPGeoLayer()                             */
/************************************************************************/

OGRPGeoLayer::~OGRPGeoLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != nullptr )
    {
        CPLDebug( "PGeo", "%d features read on layer '%s'.",
                  static_cast<int>(m_nFeaturesRead),
                  poFeatureDefn->GetName() );
    }

    if( poStmt != nullptr )
    {
        delete poStmt;
        poStmt = nullptr;
    }

    if( poFeatureDefn != nullptr )
    {
        poFeatureDefn->Release();
        poFeatureDefn = nullptr;
    }

    CPLFree( pszGeomColumn );
    CPLFree( panFieldOrdinals );
    CPLFree( pszFIDColumn );

    if( poSRS != nullptr )
    {
        poSRS->Release();
        poSRS = nullptr;
    }
}

/************************************************************************/
/*                          BuildFeatureDefn()                          */
/*                                                                      */
/*      Build feature definition from a set of column definitions       */
/*      set on a statement.  Sift out geometry and FID fields.          */
/************************************************************************/

CPLErr OGRPGeoLayer::BuildFeatureDefn( const char *pszLayerName,
                                       CPLODBCStatement *poStmtIn )

{
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    int    nRawColumns = poStmtIn->GetColCount();

    poFeatureDefn->Reference();
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * nRawColumns );

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        OGRFieldDefn    oField( poStmtIn->GetColName(iCol), OFTString );

        oField.SetWidth(
            std::max(static_cast<short>(0), poStmtIn->GetColSize( iCol )));

        if( pszGeomColumn != nullptr
            && EQUAL(poStmtIn->GetColName(iCol),pszGeomColumn) )
            continue;

        if( pszFIDColumn == nullptr
            && EQUAL(poStmtIn->GetColName(iCol),"OBJECTID") )
        {
            pszFIDColumn = CPLStrdup(poStmtIn->GetColName(iCol));
        }

        if( pszGeomColumn == nullptr
            && EQUAL(poStmtIn->GetColName(iCol),"Shape") )
        {
            pszGeomColumn = CPLStrdup(poStmtIn->GetColName(iCol));
            continue;
        }

        switch( poStmtIn->GetColType(iCol) )
        {
          case SQL_INTEGER:
          case SQL_SMALLINT:
            oField.SetType( OFTInteger );
            break;

          case SQL_BINARY:
          case SQL_VARBINARY:
          case SQL_LONGVARBINARY:
            oField.SetType( OFTBinary );
            break;

          case SQL_DECIMAL:
            oField.SetType( OFTReal );
            oField.SetPrecision( poStmtIn->GetColPrecision(iCol) );
            break;

          case SQL_FLOAT:
          case SQL_REAL:
          case SQL_DOUBLE:
            oField.SetType( OFTReal );
            oField.SetWidth( 0 );
            break;

          case SQL_C_DATE:
            oField.SetType( OFTDate );
            break;

          case SQL_C_TIME:
            oField.SetType( OFTTime );
            break;

          case SQL_C_TIMESTAMP:
          case SQL_C_TYPE_TIMESTAMP:
            oField.SetType( OFTDateTime );
            break;

          default:
            /* leave it as OFTString */;
        }

        poFeatureDefn->AddFieldDefn( &oField );
        panFieldOrdinals[poFeatureDefn->GetFieldCount() - 1] = iCol+1;
    }

    if( pszGeomColumn != nullptr )
        poFeatureDefn->GetGeomFieldDefn(0)->SetName(pszGeomColumn);
    else
        poFeatureDefn->SetGeomType( wkbNone );

    return CE_None;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGeoLayer::ResetReading()

{
    iNextShapeId = 0;
    m_bEOF = false;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGeoLayer::GetNextFeature()

{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == nullptr )
            return nullptr;

        if( (m_poFilterGeom == nullptr
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == nullptr
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRPGeoLayer::GetNextRawFeature()

{
    OGRErr err = OGRERR_NONE;

    if( m_bEOF || GetStatement() == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we are marked to restart then do so, and fetch a record.     */
/* -------------------------------------------------------------------- */
    if( !poStmt->Fetch() )
    {
        delete poStmt;
        poStmt = nullptr;
        m_bEOF = true;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    if( pszFIDColumn != nullptr && poStmt->GetColId(pszFIDColumn) > -1 )
        poFeature->SetFID(
            atoi(poStmt->GetColData(poStmt->GetColId(pszFIDColumn))) );
    else
        poFeature->SetFID( iNextShapeId );

    iNextShapeId++;
    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Set the fields.                                                 */
/* -------------------------------------------------------------------- */
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        const OGRFieldType eType = poFeatureDefn->GetFieldDefn(iField)->GetType();
        int iSrcField = panFieldOrdinals[iField]-1;

        if ( eType == OFTReal && (poStmt->Flags() & CPLODBCStatement::Flag::RetrieveNumericColumnsAsDouble) )
        {
            // for OFTReal fields we retrieve the value directly as a double
            // to avoid loss of precision associated with double/float->string conversion
            const double dfValue = poStmt->GetColDataAsDouble( iSrcField );
            if ( std::isnan( dfValue ) )
            {
                poFeature->SetFieldNull( iField );
            }
            else
            {
                poFeature->SetField( iField, dfValue );
            }
        }
        else
        {
            const char *pszValue = poStmt->GetColData( iSrcField );

            if( pszValue == nullptr )
                poFeature->SetFieldNull( iField );
            else if( poFeature->GetFieldDefnRef(iField)->GetType() == OFTBinary )
                poFeature->SetField( iField,
                                     poStmt->GetColDataLength(iSrcField),
                                     (GByte *) pszValue );
            else
                poFeature->SetField( iField, pszValue );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to extract a geometry.                                      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != nullptr )
    {
        int iField = poStmt->GetColId( pszGeomColumn );
        GByte *pabyShape = (GByte *) poStmt->GetColData( iField );
        int nBytes = poStmt->GetColDataLength(iField);
        OGRGeometry *poGeom = nullptr;

        if( pabyShape != nullptr )
        {
            err = OGRCreateFromShapeBin( pabyShape, &poGeom, nBytes );
            if( OGRERR_NONE != err )
            {
                CPLDebug( "PGeo",
                          "Translation shape binary to OGR geometry failed (FID=%ld)",
                           (long)poFeature->GetFID() );
            }
        }

        if( poGeom != nullptr && OGRERR_NONE == err )
        {
            // always promote polygon/linestring geometries to multipolygon/multilinestring,
            // so that the geometry types returned for the layer are predictable and match
            // the advertised layer geometry type. See more details in OGRPGeoTableLayer::Initialize
            const OGRwkbGeometryType eFlattenType = wkbFlatten(poGeom->getGeometryType());
            if( eFlattenType == wkbPolygon || eFlattenType == wkbLineString )
                poGeom = OGRGeometryFactory::forceTo(poGeom, OGR_GT_GetCollection(poGeom->getGeometryType()));

            poGeom->assignSpatialReference( poSRS );
            poFeature->SetGeometryDirectly( poGeom );
        }
    }

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPGeoLayer::GetFeature( GIntBig nFeatureId )

{
    /* This should be implemented directly! */

    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGeoLayer::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

void OGRPGeoLayer::LookupSRID( int nSRID )

{
/* -------------------------------------------------------------------- */
/*      Fetch the corresponding WKT from the SpatialRef table.          */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oStmt( poDS->GetSession() );

    oStmt.Appendf( "SELECT srtext FROM GDB_SpatialRefs WHERE srid = %d",
                  nSRID );

    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "'%s' failed.\n%s",
                  oStmt.GetCommand(),
                  poDS->GetSession()->GetLastError() );
        return;
    }

    if( !oStmt.Fetch() )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "SRID %d lookup failed.\n%s",
                  nSRID, poDS->GetSession()->GetLastError() );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Check that it isn't just a GUID.  We don't know how to          */
/*      translate those.                                                */
/* -------------------------------------------------------------------- */
    const char *pszSRText = oStmt.GetColData(0);

    if( pszSRText[0] == '{' )
    {
        CPLDebug( "PGEO", "Ignoring GUID SRTEXT: %s", pszSRText );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Turn it into an OGRSpatialReference.                            */
/* -------------------------------------------------------------------- */
    poSRS = new OGRSpatialReference();
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    if( poSRS->importFromWkt( pszSRText ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "importFromWKT() failed on SRS '%s'.",
                  pszSRText);
        delete poSRS;
        poSRS = nullptr;
    }
    else
        nSRSId = nSRID;
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRPGeoLayer::GetFIDColumn()

{
    if( pszFIDColumn != nullptr )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRPGeoLayer::GetGeometryColumn()

{
    if( pszGeomColumn != nullptr )
        return pszGeomColumn;
    else
        return "";
}
