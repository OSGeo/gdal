/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeomediaLayer class, code shared between
 *           the direct table access, and the generic SQL results.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
#include "ogr_geomedia.h"
#include "cpl_string.h"
#include "ogrgeomediageometry.h"

#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRGeomediaLayer()                          */
/************************************************************************/

OGRGeomediaLayer::OGRGeomediaLayer() :
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
/*                         ~OGRGeomediaLayer()                          */
/************************************************************************/

OGRGeomediaLayer::~OGRGeomediaLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != nullptr )
    {
        CPLDebug( "Geomedia", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead,
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

CPLErr OGRGeomediaLayer::BuildFeatureDefn( const char *pszLayerName,
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

        oField.SetWidth(std::max(static_cast<short>(0),
                                 poStmtIn->GetColSize(iCol)));

        if( pszGeomColumn != nullptr
            && EQUAL(poStmtIn->GetColName(iCol),pszGeomColumn) )
            continue;

        if( pszGeomColumn == nullptr
            && EQUAL(poStmtIn->GetColName(iCol),"Geometry")
            && (poStmtIn->GetColType(iCol) == SQL_BINARY ||
                poStmtIn->GetColType(iCol) == SQL_VARBINARY ||
                poStmtIn->GetColType(iCol) == SQL_LONGVARBINARY) )
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

    return CE_None;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeomediaLayer::ResetReading()

{
    iNextShapeId = 0;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRGeomediaLayer::GetNextRawFeature()

{
    OGRErr err = OGRERR_NONE;

    if( GetStatement() == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we are marked to restart then do so, and fetch a record.     */
/* -------------------------------------------------------------------- */
    if( !poStmt->Fetch() )
    {
        delete poStmt;
        poStmt = nullptr;
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
        int iSrcField = panFieldOrdinals[iField]-1;
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
            err = OGRCreateFromGeomedia( pabyShape, &poGeom, nBytes );
            if( OGRERR_NONE != err )
            {
                CPLDebug( "Geomedia",
                          "Translation geomedia binary to OGR geometry failed (FID=%ld)",
                           (long)poFeature->GetFID() );
            }
        }

        if( poGeom != nullptr && OGRERR_NONE == err )
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

OGRFeature *OGRGeomediaLayer::GetFeature( GIntBig nFeatureId )

{
    /* This should be implemented directly! */

    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeomediaLayer::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRGeomediaLayer::GetFIDColumn()

{
    if( pszFIDColumn != nullptr )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRGeomediaLayer::GetGeometryColumn()

{
    if( pszGeomColumn != nullptr )
        return pszGeomColumn;
    else
        return "";
}
