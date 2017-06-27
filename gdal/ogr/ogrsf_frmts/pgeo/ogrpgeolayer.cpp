/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGeoLayer class, code shared between
 *           the direct table access, and the generic SQL results.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
    poFeatureDefn(NULL),
    poStmt(NULL),
    poSRS(NULL),
    nSRSId(-2), // we haven't even queried the database for it yet.
    iNextShapeId(0),
    poDS(NULL),
    pszGeomColumn(NULL),
    pszFIDColumn(NULL),
    panFieldOrdinals(NULL)
{}

/************************************************************************/
/*                            ~OGRPGeoLayer()                             */
/************************************************************************/

OGRPGeoLayer::~OGRPGeoLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "PGeo", "%d features read on layer '%s'.",
                  static_cast<int>(m_nFeaturesRead),
                  poFeatureDefn->GetName() );
    }

    if( poStmt != NULL )
    {
        delete poStmt;
        poStmt = NULL;
    }

    if( poFeatureDefn != NULL )
    {
        poFeatureDefn->Release();
        poFeatureDefn = NULL;
    }

    CPLFree( pszGeomColumn );
    CPLFree( panFieldOrdinals );
    CPLFree( pszFIDColumn );

    if( poSRS != NULL )
    {
        poSRS->Release();
        poSRS = NULL;
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

        if( pszGeomColumn != NULL
            && EQUAL(poStmtIn->GetColName(iCol),pszGeomColumn) )
            continue;

        if( pszFIDColumn == NULL
            && EQUAL(poStmtIn->GetColName(iCol),"OBJECTID") )
        {
            pszFIDColumn = CPLStrdup(poStmtIn->GetColName(iCol));
        }

        if( pszGeomColumn == NULL
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
            oField.SetType( OFTDateTime );
            break;

          default:
            /* leave it as OFTString */;
        }

        if( pszGeomColumn != NULL )
            poFeatureDefn->GetGeomFieldDefn(0)->SetName(pszGeomColumn);

        poFeatureDefn->AddFieldDefn( &oField );
        panFieldOrdinals[poFeatureDefn->GetFieldCount() - 1] = iCol+1;
    }

    return CE_None;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGeoLayer::ResetReading()

{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGeoLayer::GetNextFeature()

{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
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

    if( GetStatement() == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If we are marked to restart then do so, and fetch a record.     */
/* -------------------------------------------------------------------- */
    if( !poStmt->Fetch() )
    {
        delete poStmt;
        poStmt = NULL;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    if( pszFIDColumn != NULL && poStmt->GetColId(pszFIDColumn) > -1 )
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
        int iSrcField = panFieldOrdinals[iField]-1;
        const char *pszValue = poStmt->GetColData( iSrcField );

        if( pszValue == NULL )
            poFeature->SetFieldNull( iField );
        else if( poFeature->GetFieldDefnRef(iField)->GetType() == OFTBinary )
            poFeature->SetField( iField,
                                 poStmt->GetColDataLength(iSrcField),
                                 (GByte *) pszValue );
        else
            poFeature->SetField( iField, pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Try to extract a geometry.                                      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != NULL )
    {
        int iField = poStmt->GetColId( pszGeomColumn );
        GByte *pabyShape = (GByte *) poStmt->GetColData( iField );
        int nBytes = poStmt->GetColDataLength(iField);
        OGRGeometry *poGeom = NULL;

        if( pabyShape != NULL )
        {
            err = OGRCreateFromShapeBin( pabyShape, &poGeom, nBytes );
            if( OGRERR_NONE != err )
            {
                CPLDebug( "PGeo",
                          "Translation shape binary to OGR geometry failed (FID=%ld)",
                           (long)poFeature->GetFID() );
            }
        }

        if( poGeom != NULL && OGRERR_NONE == err )
        {
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
    char *pszSRText = (char *) oStmt.GetColData(0);

    if( pszSRText[0] == '{' )
    {
        CPLDebug( "PGEO", "Ignoreing GUID SRTEXT: %s", pszSRText );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Turn it into an OGRSpatialReference.                            */
/* -------------------------------------------------------------------- */
    poSRS = new OGRSpatialReference();

    if( poSRS->importFromWkt( &pszSRText ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "importFromWKT() failed on SRS '%s'.",
                  pszSRText);
        delete poSRS;
        poSRS = NULL;
    }
    else if( poSRS->morphFromESRI() != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "morphFromESRI() failed on SRS." );
        delete poSRS;
        poSRS = NULL;
    }
    else
        nSRSId = nSRID;
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRPGeoLayer::GetFIDColumn()

{
    if( pszFIDColumn != NULL )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRPGeoLayer::GetGeometryColumn()

{
    if( pszGeomColumn != NULL )
        return pszGeomColumn;
    else
        return "";
}
